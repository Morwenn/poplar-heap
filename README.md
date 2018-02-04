poplar-heap is a collection of heap algorithms programmed in C++14 whose signatures more or less conrrespond to those
of the standard library's heap algorithms. However, they use what is known as a "poplar heap" instead of a traditional
binary heap to store the data. A poplar heap is a data structure introduced by Coenraad Bron and Wim H. Hesselink in
their paper *Smoothsort revisited*. We will first describe the library interface, then explain what a poplar heap is,
and how to implement and improve the usual heap operations with poplar heaps.

Now, let's be real: compared to usual binary heap-based based functions, poplar heap-based functions are slow. This
library does not mean to provide performant algorithms. Its goals are different:
* Explaining what poplar heaps are
* Showing how poplar heaps can be implemented with O(1) extra space
* Showing that operations used in poplar sort can be decoupled
* Providing a proof-of-concept implementation

# The heap algorithms

The following functions are provided by the poplar-heap library:

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void push_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare compare={});
```

*Requires:* The range `[first, last - 1)` shall be a valid poplar heap. The type of `*first` shall satisfy the
`MoveConstructible` requirements and the `MoveAssignable` requirements.

*Effects:* Places the value in the location `last - 1` into the resulting poplar heap `[first, last)`.

*Complexity:* At most log(`last - first`) comparisons.

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void pop_heap(RandomAccessIterator first, RandomAccessIterator last,
              Compare compare={});
```

*Requires:* The range `[first, last)` shall be a valid non-empty poplar heap. `RandomAccessIterator` shall satisfy the
requirements of `ValueSwappable`. The type of `*first` shall satisfy the requirements of `MoveConstructible` and of
`MoveAssignable`.

*Effects:* Swaps the highest value in `[first, last)` with the value in the location `last - 1` and makes
`[first, last - 1)` into a poplar heap.

*Complexity:* O(log(`last - first`)) comparisons.

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void make_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare compare={});
```

*Requires:* The type of `*first` shall satisfy the `MoveConstructible` requirements and the `MoveAssignable`
requirements.

*Effects:* Constructs a poplar heap out of the range `[first, last)`.

*Complexity:* O(*N* log(*N*)) comparisons, where *N* = `last - first`.

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void sort_heap(RandomAccessIterator first, RandomAccessIterator last,
               Compare compare={});
```

*Requires:* The range `[first, last)` shall be a valid poplar heap. `RandomAccessIterator` shall satisfy the
requirements of `ValueSwappable`. The type of `*first` shall satisfy the requirements of `MoveConstructible` and of
`MoveAssignable`.

*Effects:* Sorts elements in the poplar heap `[first, last)`.

*Complexity:* O(*N* log(*N*)) comparisons, where *N* = `last - first`.

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void is_heap(RandomAccessIterator first, RandomAccessIterator last,
             Compare compare={});
```

*Returns:* `is_heap_until(first, last, compare) == last`.

```cpp
template<
    typename RandomAccessIterator,
    typename Compare = std::less<>
>
void is_heap_until(RandomAccessIterator first, RandomAccessIterator last,
                   Compare compare={});
```

*Returns:* If `(last - first) < 2`, returns `last`. Otherwise, returns the last iterator `it` in `[first, last]` for
which the range `[first, it)` is a poplar heap.

*Complexity:* O(`last - first`).

# Poplar heap

### Poplars

Poplar sort is a heapsort-like algorithm derived from smoothsort that builds a forest of specific trees named
"poplars" before sorting them. The structure in described as follows in the original *Smoothsort Revisited* paper:

> Let us first define a heap to be a binary tree having its maximal element in the root and having two subtrees each of
which is empty or a heap. A heap is called perfect if both subtrees are empty or perfect heaps of the same size. A
poplar is defined to be a perfect heap mapped on a contiguous section of the array in the form of two subpoplars (or
empty sections) followed by the root.

Because of its specific structure, we can already intuitively note that the size of a poplar is always a power of two
minus one. This property is extensively used in the algorithm. The following graph represents a poplar containing seven
elements, and shows how it is mapped to an array:

![Poplar containing 7 elements](https://cdn.rawgit.com/Morwenn/poplar-heap/master/graphs/poplar.png)

Now, let us define a "poplar heap" to be a forest of poplars organized in such a way that the bigger poplars come first
and the smaller poplars come last. Moreover, the poplars should be as big as they possibly can. For example if a poplar
heap contains 12 elements, it will be made of 4 poplars with respectively 7, 3, 1 and 1 elements. Two properties of
poplar heaps described in the original paper are worth mentioning:

* There can't be more than O(log n) poplars in a poplar heap of n elements.
* Only the last two poplars can have a similar size.

![Poplar containing 7 elements](https://cdn.rawgit.com/Morwenn/poplar-heap/master/graphs/poplar-heap.png)

Another interesting property of poplar heaps is that a sorted collection is a valid poplar heap. One of the main ideas
behind poplar sort was that an almost sorted collection would be faster to sort because constructing the poplar heap
wouldn't move many elements around, while a regular heapsort can't take advantage of presortedness at all. We will see
later that this property can actually be used to perform a few optimizations.

### Semipoplars

To handle poplars whose root has been replaced, Bron & Hesselink introduce the concept of semipoplar: a semipoplar has
the same properties as a poplar except that the root can be smaller than be smaller than the roots of the subtrees. A
semipoplar is mostly useful to represent an intermediate case where we are building a bigger poplar from two subpoplars
and a root. Here is an example of a semipoplar:

![Semipoplar containing 7 elements](https://cdn.rawgit.com/Morwenn/poplar-heap/master/graphs/semipoplar.png)

A semipoplar can be transformed into a poplar thanks to a procedure called *sift*, which is actually pretty close from
the equivalent procedure in heapsort: if the root of the semipoplar is smaller than that of a subpoplar, swap it with
the bigger of the two subpoplar roots, and recursively call *sift* on the subpoplar whose root has been swapped until
the whole thing becomes a poplar again. A naive C++ implementation of the algorithm would look like this (to avoid
boilerplate, we don't template the examples on the comparison operators):

```cpp
/**
 * Transform a semipoplar into a poplar
 *
 * @param first Iterator to the first element of the semipoplar
 * @param size Size of the semipoplar
 */
template<typename Iterator, typename Size>
void sift(Iterator first, Size size)
{
    if (size < 2) return;

    // Find the root of the semipoplar and those of the subpoplars
    auto root = first + (size - 1);
    auto child_root1 = root - 1;
    auto child_root2 = first + (size / 2 - 1);

    // Pick the bigger of the roots
    auto max_root = root;
    if (*max_root < *child_root1) {
        max_root = child_root1;
    }
    if (*max_root, *child_root2) {
        max_root = child_root2;
    }

    // If one of the roots of the subpoplars was the biggest of all, swap it
    // with the root of the semipoplar, and recursively call sift of this
    // subpoplar
    if (max_root != root) {
        std::iter_swap(root, max_root);
        sift(max_root - (size / 2 - 1), size / 2);
    }
}
```

# Poplar sort

As most heapsort-like algorithms, poplar is divided into two main parts:

* Turning the passed collection into a poplar heap
* Sorting the poplar heap

While making a binary heap can be made in O(n) time, I don't know of any way to make a poplar heap with anything better
than O(n log n) time. In both cases, sorting the heap dominates the algorithm and runs in O(n log n) time. Be it the
original algorithm described by Bron & Hesselink or the revisited algorithm that I will describe later, both are split
into these two distinct phases.

## Original poplar sort

The original poplar sort algorithm actually stores up to log2(n) integers to represent the positions of the poplars. We
will use (and store) the following small structure instead to represent a poplar in order to simplify the understanding
of the algorithm while preserving the original logic as well as the original space and time complexities:

```cpp
template<typename Iterator>
struct poplar
{
    // The poplar is located at the position [begin, end) in the memory
    Iterator begin, end;
    // Unsigned integers because we're doing to perform bit tricks
    std::make_unsigned_t<typename std::iterator_traits<Iterator>::difference_type> size;

    auto root() const
        -> Iterator
    {
        // The root of a poplar is always its last element
        return std::prev(res);
    }
};
```

With that structure we can easily make an array of poplars to represent the poplar heap. Storing both the beginning,
end and size of a poplar is a bit redundant, but in my benchmarks it was what ended being the fastest at the end of the
day: apparently computing them again and again wasn't the best solution.

The poplar heap is constructed iteratively: elements are added to the poplar heap one at a time. Whenever such an
element is added, it is first added as a single-element poplar at the end of the heap. Then, if the previous two
poplars have the same size, both of them are combined in a bigger semipoplar where the new element serves as the root,
and the *sift* procedure is applied to turn the new semipoplar into a full-fledged poplar. The first part of the poplar
sort algorithm thus looks like this:

```cpp
template<typename Iterator>
void poplar_sort(Iterator first, Iterator last)
{
    auto size = std::distance(first, last);
    if (size < 2) return;

    // Poplars forming the poplar heap
    std::vector<poplar<Iterator>> poplars;

    // Make a poplar heap
    for (auto it = first ; it != last ; ++it) {
        auto nb_pops = poplars.size();
        if (nb_pops >= 2 && poplars[nb_pops-1].size == poplars[nb_pops-2].size) {
            // Find the bounds of the new semipoplar
            auto begin = poplars[nb_pops-2].begin;
            auto end = std::next(it);
            auto poplar_size = 2 * poplars[nb_pops-2].size + 1;
            // Fuse the last two poplars and the new element into a semipoplar
            poplars.pop();
            poplars.pop();
            poplars.push_back({begin, end, poplar_size});
            // Turn the new semipoplar into a full-fledged poplar
            sift(begin, poplar_size);
        } else {
            // Add the new element as a single-element poplar
            poplars.push_back({it, std::next(it), 1});
        }
    }

    // TODO: sort the poplar heap
}
```

Now that we have our poplar heap, it's time to sort it. Just like a regular heapsort it's done by popping elements from
the poplar heap one by one. Popping an element works as follows:

* Switch the biggest of the poplar roots with the root of the last poplar
* Apply the *sift* procedure to the poplar whose root has been taken to restore the poplar invariants
* Remove the last element from the heap
* If that element formed a single-element poplar, we are done
* Otherwise split the rest of the last poplar into two poplars of equal size

The first two steps are known as the *relocate* procedure in the original paper, which can be roughly implemented as
follows:

```cpp
template<typename Iterator>
void relocate(std::vector<poplar<Iterator>>& poplars)
{
    // Find the poplar with the bigger root, assuming that there is
    // always at least one poplar in the vector
    auto last = std::prev(std::end(poplars));
    auto bigger = last;
    for (auto it = std::begin(poplars) ; it != last ; ++it) {
        if (*bigger->root() < *it->root()) {
            bigger = it;
        }
    }

    // Swap & sift if needed
    if (bigger != last) {
        std::iter_swap(bigger->root(), last->root());
        sift(bigger->begin, bigger->size);
    }
}
```

The loop to sort the poplar heap element by element (which replaces our previous TODO comment in `poplar_sort`) looks
like this:

```cpp
// Sort the poplar heap
do {
    relocate(poplars);
    if (poplars.back().size == 1) {
        poplars.pop_back();
    } else {
        // Find bounds of the new poplars
        auto poplar_size = poplars.back().size / 2;
        auto begin1 = poplars.back().begin;
        auto begin2 = begin1 + size;
        // Split the poplar in two poplars, don't keep the last element
        poplars.pop_back();
        poplars.push_back({begin1, begin2, poplar_size});
        poplars.push_back({begin2, begin2 + poplar_size, poplar_size});
    }
} while (poplars.size() > 0);
```

And that's pretty much it for the original poplar sort; I hope that it my explanation was understandable enough. If
something wasn't clear, don't hesitate to mention it, open an issue and/or suggest improvements to the wording.

## Poplar sort revisited: heap operations with O(1) extra space

TODO: push_heap, pop_heap, make_heap, sort_heap

## Additional poplar heap algorithms

TODO: is_heap_until, is_heap
