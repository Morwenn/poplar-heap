/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Morwenn
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
        auto hyperfloor(Unsigned n)
            -> Unsigned
        {
            static constexpr auto bound = std::numeric_limits<Unsigned>::digits / 2;
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
        auto make_poplar(RandomAccessIterator first, RandomAccessIterator last,
                         Size size, Compare compare)
            -> void
        {
            if (size < 2) return;

            if (size < 16) {
                // A sorted collection is a valid poplar heap;
                // when the heap is small, using insertion sort
                // should be faster
                unchecked_insertion_sort(std::move(first), std::move(last), std::move(compare));
                return;
            }

            using poplar_level_t = std::make_unsigned_t<
                typename std::iterator_traits<RandomAccessIterator>::difference_type
            >;
            poplar_level_t poplar_level = 1;

            auto it = first;
            auto next = std::next(it, 15);
            while (true) {
                // Make a 15 element poplar
                unchecked_insertion_sort(it, next, compare);

                // Sift elements to collapse poplars
                Size poplar_size = 15;
                auto begin = it;
                // The loop increment follows the binary carry sequence for some reason
                for (auto i = (poplar_level & -poplar_level) >> 1 ; i != 0 ; i >>= 1) {
                    begin -= poplar_size;
                    poplar_size = 2 * poplar_size + 1;
                    sift(begin, poplar_size, compare);
                    ++next;
                }

                if (next == last) return;
                it = next;
                std::advance(next, 15);
                ++poplar_level;
            }
        }

        template<typename RandomAccessIterator, typename Size, typename Compare>
        auto pop_heap_with_size(RandomAccessIterator first, RandomAccessIterator last,
                                Size size, Compare compare)
            -> void
        {
            auto poplar_size = detail::hyperfloor(size) - 1;
            auto last_root = std::prev(last);
            auto bigger = last_root;
            auto bigger_size = poplar_size;

            // Look for the bigger poplar root
            auto it = first;
            while (true) {
                if (Size(std::distance(it, last)) >= poplar_size) {
                    auto root = it + (poplar_size - 1);
                    if (root == last_root) break;
                    if (compare(*bigger, *root)) {
                        bigger = root;
                        bigger_size = poplar_size;
                    }
                    it = std::next(root);
                } else {
                    poplar_size = (poplar_size + 1) / 2 - 1;
                }
            }

            // If a poplar root was bigger than the last one, exchange
            // them and sift
            if (bigger != last_root) {
                using std::swap;
                swap(*bigger, *last_root);
                sift(bigger - (bigger_size - 1), bigger_size, compare);
            }
        }
    }

    ////////////////////////////////////////////////////////////
    // Standard-library-style make_heap and sort_heap
    ////////////////////////////////////////////////////////////

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
        using poplar_size_t = std::make_unsigned_t<
            typename std::iterator_traits<RandomAccessIterator>::difference_type
        >;
        poplar_size_t size = std::distance(first, last);
        if (size < 2) return;

        auto poplar_size = detail::hyperfloor(size) - 1;
        auto it = first;
        do {
            if (poplar_size_t(std::distance(it, last)) >= poplar_size) {
                auto begin = it;
                auto end = it + poplar_size;
                detail::make_poplar(begin, end, poplar_size, compare);
                it = end;
            } else {
                poplar_size = (poplar_size + 1) / 2 - 1;
            }
        } while (poplar_size > 0);
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
}

#endif // POPLAR_HEAP_H_
