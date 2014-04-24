/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Rochester Mafia",
    /* First member's full name */
    "H. Antonio Cardenas",
    /* First member's email address */
    "hcardena@u.rochester.edu",
    /* Second member's full name (leave blank if none) */
    "Jacob W. Brock",
    /* Second member's email address (leave blank if none) */
    "jbrock@cs.rocheter.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Global Variables*/
static char * mem_heap; /* Ptr to first byte of the heap */
static char * mem_brk; /* Ptr to last byte of the heap + 1 */
static char * mem_max_addr; /* Max legal heap addr + 1 */
static char * heap_listp;

/*Figure 9.43 from textbook (pg. 830) tips*/

/*Basic constants and macros*/
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */
#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
//#define PACK(size, alloc) ((size) | (alloc))
#define PACK(size, prev_alloc, alloc) ((size) | (prev_alloc << 1) | (alloc)) //jake

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1) //jake

/* Given block ptr bp, computer address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*Max Heap Size */
#define MAX_HEAP (20*(1<<20)) /*20 MB*/

/* coalesce and mm_free from Figure 9.46 in book. */

static void *coalesce(void *bp)
{
  // Assumes that, in the end, the previous block's previous block is allocated
  // (otherwise it already would have been coalesced).

  //  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));  

  if (prev_alloc && next_alloc) { /* Case 1 */
    return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2 */
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 1, 0));
    PUT(FTRP(bp), PACK(size, 1, 0));
  }
  
  else if (!prev_alloc && next_alloc) { /* Case 3 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))); //could optimize by getting from footer
    PUT(FTRP(bp), PACK(size, 1, 0)); //assumes previous-previous blk is allocated.
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));
    bp = PREV_BLKP(bp);
  }
  
  else { /* Case 4 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1, 0));
    bp = PREV_BLKP(bp);
  }

  return bp;
}

void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));

  size_t next_blk_size; //jake
  size_t next_blk_alloc; //jake

  PUT(HDRP(bp), PACK(size, prev_alloc, 0)); // Is this necessary?
  PUT(FTRP(bp), PACK(size, prev_alloc, 0));
  
  bp = coalesce(bp);

  // Inform next block I'm free. //jake
  next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
  next_blk_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(next_blk_size, 0, next_blk_alloc));
}

/* extend_heap from Figure 9.45 in book. */
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;
  size_t prev_blk_alloc;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  /* Initialize free block header/footer and the epilogue header */
  prev_blk_alloc = GET_PREV_ALLOC(HDRP(NEXT_BLKP(bp))); /* Get that from epilogue header. */
  PUT(HDRP(bp), PACK(size, prev_blk_alloc, 0)); /* Free block header */
  PUT(FTRP(bp), PACK(size, prev_blk_alloc, 0)); /* Free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1)); /* New epilogue header */

  /* Coalesce if the previous block was free */
  if (!prev_blk_alloc)
    return coalesce(bp);
  else
    return bp;
}
  
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  // I don't think we need a prologue since we have the prev_alloc bit.

  size_t first_size;
  size_t first_alloc;
  size_t first_prev_alloc = 1;

  if((heap_listp = mem_sbrk(2*WSIZE)) == (void *)-1) // May only need 2*WSIZE.
    return -1;
  PUT(heap_listp, 0); /* Alignment padding */
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1, 1)); /* Prologue header (ignore footer)*/
  PUT(heap_listp + (3*WSIZE), PACK(0, 1, 1)); /* Epilogue header */
  heap_listp += (2*WSIZE); /* Put heap_listp right after the prologue block. */

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return -1;
  return 0;
}

/* find_fit from Problem 9.8 in book. */
static void *find_fit(size_t asize)
{
  /* First fit search */
  void *bp;

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
      return bp;
    }
  }
  return NULL; /* No fit */
}

/* place from Problem 9.9 in book. */
static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));
  size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
  size_t next_blk_size;
  size_t next_blk_alloc;

  if ((csize - asize) >= (DSIZE)) { /* If enough extra space for second block to be made: */
    PUT(HDRP(bp), PACK(asize, prev_alloc, 1)); /* Make header. */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 1, 0)); /* Write header of second block. */
    PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 1, 0)); /* Write footer of second block. */
  }
  else {
    PUT(HDRP(bp), PACK(csize, prev_alloc, 1)); /* Make header. */

    // Inform next block I'm allocated.
    next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(bp))); 
    next_blk_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(next_blk_size, 1, next_blk_alloc));
  }
}

/* mm_malloc from Figure 9.47 in book. */
void *mm_malloc(size_t size)
{
  size_t asize; /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;
  
  /* Ignore spurious requests */
  if (size == 0)
    return NULL;
  
  /* Adjust block size to include overhead and alignment reqs. */
    asize = ALIGN(4 + size);

  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }
  
  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize,CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
  place(bp, asize);
  return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;
  
  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;

  copySize = GET_SIZE(HDRP(oldptr)) - 4; // Header is 4B.
  //  copySize = *(size_t *)((char *)oldptr SIZE_T_SIZE);
  if (size < copySize)
    copySize = size;

  memcpy(newptr, oldptr, copySize);

  mm_free(oldptr);
  return newptr;
}







//GRAVEYARD---OOOOOOOOHHHHHH
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
/*
void *mm_malloc(size_t size)
{
  int newsize = ALIGN(size + SIZE_T_SIZE);
  void *p = mem_sbrk(newsize);
  if (p == (void *)-1)
    return NULL;
  else {
    *(size_t *)p = size;
    return (void *)((char *)p + SIZE_T_SIZE);
  }
}
*/

/* find_fit modified from Problem 9.8 in book. */

//static void *next_find_fit(size_t asize)
//{
//  /* Next fit search */
// void *bp;

//for (bp = moving_heap_listp ; ; bp = NEXT_BLKP(bp)) {
//  if (GET_SIZE(HDRP(bp)) == 0)
//    bp = heap_listp;
//
//  if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
//    moving_heap_listp = NEXT_BLKP(bp);
//    return bp;
//  }
//
//  if (bp == PREV_BLKP(moving_heap_listp))
//    return NULL;
//}
//return NULL; /* No fit */
//}

