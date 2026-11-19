# malloc: a custom heap allocator

**Name:** EDWIN WAWERU  
**Student ID:** SCT212-0110/2022  
**Course:** ICS 2305 Systems Programming

## Build and run

```bash
make            # builds and runs the provided test harness
make bench      # builds bench.c and runs first-fit, best-fit and glibc
make clean
```

Sanitizer run:

```bash
gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```

## Tasks

| Task | Description | Status |
|---|---|---|
| 1 | Block header, `align_up`, `payload`, `header`, `HDR` | Complete |
| 2 | `grow()` with `sbrk`, 64 KB minimum, alignment padding | Complete |
| 3 | `my_malloc` with first-fit | Complete |
| 4 | Block splitting | Complete |
| 5 | `my_free` with invalid-pointer and double-free detection, coalescing | Complete |
| 6 | `my_calloc` (overflow check) and `my_realloc` (all five cases) | Complete |
| 7 | Best-fit, `heap_stats`, `heap_dump`, `heap_check` | Complete |
| 8 | `bench.c` and `REPORT.md` | Complete |

## Test results

Last lines of `make` (the same result under `-fsanitize=address,undefined`):

```
  PASS  fully coalesced (few free blocks remain)
  PASS  heap consistent

Result: 29 passed, 0 failed
```

## Design

Each block header contains a size, a next pointer, a free flag and a magic number. As block_t is 24 bytes, HDR is defined as 32 bytes to make the header divisible by 16. The data starts HDR bytes after the start of the block, so subtracting HDR from the pointer passed to my_free gives the start of the block header. Offsets use `char *` math, so they are in bytes.

All blocks are stored in a singly-linked list ordered by ascending addresses. If no free block fits, a chunk of memory is obtained with sbrk() (at least 64KB) and added to the end of the list. If the current program break is not divisible by 16, padding is added first, and the new block becomes a free block. If the new memory is directly after a free tail block, that tail block is extended.

my_malloc rounds the requested size up to be divisible by 16. A suitable block is found using either first-fit or best-fit. If the remaining space can fit a block header and 16B, the block is split, and the remainder becomes a free block directly after.

If the pointer passed to my_free is not correctly aligned, not in the heap, or has the wrong magic number, it is identified as an invalid pointer. If the block is already free, it is identified as a double-free. Otherwise, the block is marked as free and the list is traversed. If two free blocks are adjacent (the first block's payload ends where the second block's header starts), they are merged. The first block's size increases by HDR plus the second block's size, and the second block is removed from the list. If two blocks are consecutive in the list but not in memory, they are not merged.

## Questions

**Q1) Why must the header size be a multiple of 16, as well as the data sizes?**

If HDR were 24 bytes, then there would be 8 bytes of misalignment in the data. As the header of the next block directly follows the data of the previous block, data must be a multiple of 16 in size. If HDR and the data sizes are a multiple of 16, all headers and data in blocks will be correctly aligned.

**Q2) Why can calling printf() in my_malloc() cause infinite recursion?**

As printf() uses stdio buffers in the C library, which are allocated using malloc(), if my_malloc() is the program's malloc, then by calling printf(), my_malloc() will be called again, causing infinite recursion. Even without infinite recursion, the list may not be fully updated at that point. Therefore, calling my_malloc() again (re-entrancy) could corrupt the list. Only snprintf() and write() are used within the allocator.

**Q3) You have implemented my_free() with time complexity O(n). What additional information could be added to allow finding previous blocks in O(1) time? What is this technique known as?**

By adding a footer to the end of each block, information of block size and whether the block is free can be stored. From the header of a block, the footer of the previous block can be reached in constant time. From the footer of the previous block, the header of the previous block can be reached. This method is known as boundary tags (Knuth). Alternatively, by adding a prev pointer to the block header, and therefore creating a doubly-linked list, blocks can be merged with both their previous and next blocks in O(1) time.

**Q4) What are internal and external fragmentation? Give an example of each in your allocator.**

Internal fragmentation is wasted space inside allocated blocks. External fragmentation is when there is a large amount of free memory, but it is made up of many free blocks that are not contiguous, and none are large enough to satisfy a particular request. In this allocator, internal fragmentation comes from rounding the allocation size up (align_up), meaning that a 100-byte request will be rounded up to 112 bytes, or allocating an entire block even though there is insufficient space left to split. External fragmentation comes from splitting blocks, and freeing blocks to create gaps in memory between allocated blocks. External fragmentation in this allocator is calculated as 1 - (largest_free/free_bytes).

**Q5) Why is memory allocated via sbrk() never returned to the OS? How could it be?**

As my_free() only marks blocks as free and merges them, it never calls sbrk() with a negative value. Therefore the program break will never decrease. However, once blocks have been merged, the allocator could check if the tail block is free, and that the end address of the tail block matches the program break (using sbrk(0)). If so, and if the size of the block is above a certain size, sbrk() could be called with a negative value to reduce the program break. The tail block, list, and heap_bytes would then need to be updated. It is very important to check that the end of the block matches the current program break. Since the last time grow() was called, the C library may have modified the program break. If it is reduced incorrectly, memory that was not allocated by this allocator may be returned to the OS.

**Q6) Suppose two blocks exist consecutively in the list, but not consecutively in memory. How has this happened? How is this handled in your coalesce function?**

By calling printf() and puts() in the test harness, the C library's malloc will be called, which will change the program break. Therefore, when grow() is called, the new memory will be placed after the memory allocated by the C library. As a result, there will be a gap between the previous tail block and the new block. As the coalesce function only merges blocks that are consecutive in memory (end of data of first block equals start of header of second block) using the adjacent helper function, these blocks will never be merged.

## Known limitations

- Time complexity of my_malloc and my_free is O(n). All blocks are stored in a single list, and so the whole list must be scanned during coalescing.
- Memory is never returned to the OS (see Q5).
- No thread safety. No mutex locks are used to protect the global list.
- Invalid pointers can only be determined heuristically. Pointers into the middle of a block, or pointing to memory with a matching magic number, may not be detected.
- If a block has been merged with an adjacent block, freeing it again prints an invalid pointer message instead of a double-free message. This is because the block header is no longer valid.
- Requests larger than half of SIZE_MAX will be denied (NULL is returned).

## Sources

- Assignment brief and memory management notes (ICS 2305)
- Linux man pages: sbrk(2), brk(2), write(2), clock_gettime(2).

## AI tool used

Claude (Anthropic) was used as a teaching assistant. I wrote the report discussion, and the design explanation and answers to questions in this README, in my own words. 
