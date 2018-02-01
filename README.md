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

## The heap algorithms

The following functions are provided by the poplar-heap library:

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

## Poplar heap

Poplar sort is a heapsort-like algorithm derived from smoothsort that builds of forest of specific trees named
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
and the smaller poplars come last. Moreover, the poplars should be as big as they possinly can. For example if a poplar
heap contains 12 elements, it will be made of 4 poplars with respectively 7, 3, 1 and 1 elements. Two properties of
poplar heaps described in the original paper are worth mentioning:

* There can't be more than O(log n) poplars in a poplar heap of n elements.
* Only the last two poplars can have a similar size.

![Poplar containing 7 elements](https://cdn.rawgit.com/Morwenn/poplar-heap/master/graphs/poplar-heap.png)

## Poplar sort

TODO

## Poplar sort revisited: heap operations with O(1) extra space

TODO
