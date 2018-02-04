/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Morwenn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef POPLAR_HEAP_H_
#define POPLAR_HEAP_H_

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace poplar
{
    namespace detail
    {
        ////////////////////////////////////////////////////////////
        // Generic helper functions
        ////////////////////////////////////////////////////////////

        // Returns 2^floor(log2(n)), assumes n > 0
        template<typename Unsigned>
        constexpr auto hyperfloor(Unsigned n)
            -> Unsigned
        {
            constexpr auto bound = std::numeric_limits<Unsigned>::digits / 2;
            for (std::size_t i = 1 ; i <= bound ; i <<= 1) {
                n |= (n >> i);
            }
            return n & ~(n >> 1);
        }

        // Insertion sort which doesn't check for empty sequences
        template<typename BidirectionalIterator, typename Compare>
        auto unchecked_insertion_sort(BidirectionalIterator first, BidirectionalIterator last,
                            Compare compare)
            -> void
        {
            for (auto cur = std::next(first) ; cur != last ; ++cur) {
                auto sift = cur;
                auto sift_1 = std::prev(cur);

                // Compare first so we can avoid 2 moves for
                // an element already positioned correctly
                if (compare(*sift, *sift_1)) {
                    auto tmp = std::move(*sift);
                    do {
                        *sift = std::move(*sift_1);
                    } while (--sift != first && compare(tmp, *--sift_1));
                    *sift = std::move(tmp);
                }
            }
        }

        template<typename BidirectionalIterator, typename Compare>
        auto insertion_sort(BidirectionalIterator first, BidirectionalIterator last,
                            Compare compare)
            -> void
        {
            if (first == last) return;
            unchecked_insertion_sort(std::move(first), std::move(last), std::move(compare));
        }

        ////////////////////////////////////////////////////////////
        // Poplar heap specific helper functions
        ////////////////////////////////////////////////////////////

        template<typename RandomAccessIterator, typename Size, typename Compare>
        auto sift(RandomAccessIterator first, Size size, Compare compare)
            -> void
        {
            if (size < 2) return;

            auto root = first + (size - 1);
            auto child_root1 = root - 1;
            auto child_root2 = first + (size / 2 - 1);

            while (true) {
                auto max_root = root;
                if (compare(*max_root, *child_root1)) {
                    max_root = child_root1;
                }
                if (compare(*max_root, *child_root2)) {
                    max_root = child_root2;
                }
                if (max_root == root) return;

                using std::swap;
                swap(*root, *max_root);

                size /= 2;
                if (size < 2) return;

                root = max_root;
                child_root1 = root - 1;
                child_root2 = max_root + (size / 2 - size);
            }
        }

        template<typename RandomAccessIterator, typename Size, typename Compare>
        auto pop_heap_with_size(RandomAccessIterator first, RandomAccessIterator last,
                                Size size, Compare compare)
            -> void
        {
            auto poplar_size = detail::hyperfloor(size + 1u) - 1u;
            auto last_root = std::prev(last);
            auto bigger = last_root;
            auto bigger_size = poplar_size;

            // Look for the bigger poplar root
            auto it = first;
            while (true) {
                auto root = std::next(it, poplar_size - 1);
                if (root == last_root) break;
                if (compare(*bigger, *root)) {
                    bigger = root;
                    bigger_size = poplar_size;
                }
                it = std::next(root);

                size -= poplar_size;
                poplar_size = hyperfloor(size + 1u) - 1u;
            }

            // If a poplar root was bigger than the last one, exchange
            // them and sift
            if (bigger != last_root) {
                std::iter_swap(bigger, last_root);
                sift(bigger - (bigger_size - 1), bigger_size, std::move(compare));
            }
        }
    }

    ////////////////////////////////////////////////////////////
    // Standard-library-style make_heap and sort_heap
    ////////////////////////////////////////////////////////////

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto push_heap(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> void
    {
        using poplar_size_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;
        poplar_size_t size = std::distance(first, last);

        // Find the size of the poplar that will contain the new element in O(log n)
        poplar_size_t last_poplar_size = detail::hyperfloor(size + 1u) - 1u;
        while (size - last_poplar_size != 0) {
            size -= last_poplar_size;
            last_poplar_size = detail::hyperfloor(size + 1u) - 1u;
        }

        // Sift the new element in its poplar in O(log n)
        detail::sift(std::prev(last, last_poplar_size), last_poplar_size, std::move(compare));
    }

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto pop_heap(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> void
    {
        using poplar_size_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;
        poplar_size_t size = std::distance(first, last);
        detail::pop_heap_with_size(std::move(first), std::move(last),
                                   size, std::move(compare));
    }

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto make_heap(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> void
    {
        using poplar_diff_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;

        poplar_diff_t size = std::distance(first, last);
        if (size < 2) return;

        // A sorted collection is a valid poplar heap; whenever the heap
        // is small, using insertion sort should be faster, which is why
        // we start by constructing 15-element poplars instead of 1-element
        // ones as the base case
        constexpr poplar_diff_t small_poplar_size = 15;
        if (size <= small_poplar_size) {
            detail::unchecked_insertion_sort(std::move(first), std::move(last),
                                             std::move(compare));
            return;
        }

        // Determines the "level" of the biggest poplar seen so far
        poplar_diff_t poplar_level = 1;

        auto it = first;
        auto next = std::next(it, small_poplar_size);
        while (true) {
            // Make a 15 element poplar
            detail::unchecked_insertion_sort(it, next, compare);

            poplar_diff_t poplar_size = small_poplar_size;
            // The loop increment follows the binary carry sequence for some reason
            for (auto i = (poplar_level & -poplar_level) >> 1 ; i != 0 ; i >>= 1) {
                it -= poplar_size;
                poplar_size = 2 * poplar_size + 1;
                detail::sift(it, poplar_size, compare);
                ++next;
            }

            if (poplar_diff_t(std::distance(next, last)) <= small_poplar_size) {
                detail::insertion_sort(std::move(next), std::move(last),
                                       std::move(compare));
                return;
            }

            it = next;
            std::advance(next, small_poplar_size);
            ++poplar_level;
        }
    }

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto sort_heap(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> void
    {
        using poplar_size_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;
        poplar_size_t size = std::distance(first, last);
        if (size < 2) return;

        do {
            detail::pop_heap_with_size(first, last, size, compare);
            --last;
            --size;
        } while (size > 1);
    }

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto is_heap_until(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> RandomAccessIterator
    {
        if (std::distance(first, last) < 2) {
            return last;
        }

        using poplar_diff_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;
        // Determines the "level" of the biggest poplar seen so far
        poplar_diff_t poplar_level = 1;

        auto it = first;
        auto next = std::next(it);
        while (true) {
            poplar_diff_t poplar_size = 1;

            // The loop increment follows the binary carry sequence for some reason
            for (auto i = (poplar_level & -poplar_level) >> 1 ; i != 0 ; i >>= 1) {
                // Beginning and size of the poplar to track
                it -= poplar_size;
                poplar_size = 2 * poplar_size + 1;

                // Check poplar property against child roots
                auto root = it + (poplar_size - 1);
                auto child_root1 = root - 1;
                if (compare(*root, *child_root1)) {
                    return next;
                }
                auto child_root2 = it + (poplar_size / 2 - 1);
                if (compare(*root, *child_root2)) {
                    return next;
                }

                if (next == last) return last;
                ++next;
            }

            if (next == last) return last;
            it = next;
            ++next;
            ++poplar_level;
        }
    }

    template<typename RandomAccessIterator, typename Compare=std::less<>>
    auto is_heap(RandomAccessIterator first, RandomAccessIterator last, Compare compare={})
        -> bool
    {
        return poplar::is_heap_until(first, last, compare) == last;
    }
}

#endif // POPLAR_HEAP_H_
