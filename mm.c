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
//static char * mem_heap; /* Ptr to first byte of the heap */ Unused.
//static char * mem_brk; /* Ptr to last byte of the heap + 1 */ Unused.
//static char * mem_max_addr; /* Max legal heap addr + 1 */ Unused.

static char * heap_listp;
static void * free_list; /* Head of the free blocks linked list */

/*Figure 9.43 from textbook (pg. 830) tips*/

/*

Abstract Representation of our structure:
 _________________
[__Previous Free__] <- bp - GETPREV
[____Next Free____] <- bp - GETNEXT
[______Header_____] <- bp - GETHDR
[ Allocated Space ] <- bp always points here
[                 ]
[_________________]

*Header stays as close to bp as possible for consistency when allocating the extra two segments in free blocks. However, when the block is in use, the prev and next segments can be removed.
*/

/*Basic constants and macros*/
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */

/* Antonio's Mod */
#define GETNEXT 8 /* Distance from bp to Next Free Ptr */
#define GETPREV 12 /* Distance from bp to Previous Free Ptr */
#define GETHDR 4 /* Distance from bp to the Header */
#define MIN_BLOCK_SIZE 16 /* Prev, Next, HDR, Space */
#define NOPREV 0 /* fake address for head of the list */

#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */
#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, prev_alloc, alloc) ((size) | (prev_alloc << 1) | (alloc)) //jake

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define PUTA(p, val) (PUT(p, (unsigned int) val)) /* Puts pointers without parsing them as unsigned ints */

/* Antonio's Mod */
/* Given block ptr bp, computer address of next and prev free blocks */
#define PREV_FREE(bp) ((char *)(bp) - GETPREV)
#define NEXT_FREE(bp) ((char *)(bp) - GETNEXT)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1) //jake

/* Given block ptr bp, computer address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //why - DSIZE?

/* Max Heap Size */
#define MAX_HEAP (20*(1<<20)) /* 20 MB */

/* coalesce and mm_free from Figure 9.46 in book. */

void remove_list(void *bp)
{
  /* Skips current block in the list of free blocks */
  
  if(bp != free_list){
    PUTA(NEXT_FREE(PREV_FREE(bp)), PREV_FREE(bp));
    PUTA(PREV_FREE(NEXT_FREE(bp)), NEXT_FREE(bp));
  }
  else{ /* If trying to free the head of the list */
    
    /* May want to check if there is any element other than the head of the list */
    free_list = NEXT_FREE(bp);
    PUTA(PREV_FREE(NEXT_FREE(bp)), NOPREV);
    
  }
  
  /* May want to consider special case where freeing the last block of the list */
}

static void *coalesce(void *bp)
{
  // Assumes that, in the end, the previous block's previous block is allocated
  // (otherwise it already would have been coalesced).
  // Actually, we need to write a function that verifies that, it'll give us extra pts.
 
  /* I need to deal with a coalescion of two adjacent blocks, one of which is the current head of the list */
  /* There are occasional segmentation faults caused because free_list is NULL. Gotta fix that */
  //  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));  

  if (prev_alloc && next_alloc) { /* Case 1, blocks around are not free */
    
    PUTA((NEXT_FREE(bp)), free_list); /* Sets a new Head for the list, puts the prev head as second element. */
    PUTA((PREV_FREE(bp)), NOPREV); /* There's no previous for the beginning of the list */
    PUTA((PREV_FREE(free_list)), bp); /* Sets previous free from second element to Current block (First element) */
    free_list = bp; /* Changes the head of the list to this bp */
    /* free_list = bp; */
    //printf("%s | %s", bp, free_list); //debugging
    return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2, only next block is free */
    
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 1, 0));
    
    PUTA(NEXT_FREE(bp), NEXT_FREE(NEXT_BLKP(bp))); /* Copy the next free block's address into the just free'd block's next  */
    
    if(NEXT_BLKP(bp) == free_list)
      PUTA(PREV_FREE(bp), NOPREV); /* Set previous to none because this block is now the head of the list  */
    else
      PUTA(PREV_FREE(bp), PREV_FREE(NEXT_BLKP(bp)));
    
    PUTA(PREV_FREE(NEXT_FREE(NEXT_BLKP(bp))), bp); /* Set the next's block previous address value to this block's address   */
    PUTA(NEXT_FREE(PREV_FREE(NEXT_BLKP(bp))), bp);  /* Set the previous' block next address value to this block's address   */
    
    /* This means that nothing else is pointing to the "next free block", and all its previous pointers are now pointing the just freed' block */

    free_list = bp; /* make the just free'd block the new head of free list */
    
    PUT(FTRP(bp), PACK(size, 1, 0));
  }
  
  else if (!prev_alloc && next_alloc) { /* Case 3, only prev block is free */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))); //could optimize by getting from footer
    PUT(FTRP(bp), PACK(size, 1, 0)); //assumes previous-previous blk is allocated.

    /* The prev block is already on the free list, do NOTHING :) */
    
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));    
  }
  
  else { /* Case 4, both blocks are free */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));
    
    remove_list(NEXT_BLKP(bp)); /* Removes next block from the free list */
    
    /* Previous block is still on the free list */
    
    bp = PREV_BLKP(bp);
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1, 0));
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
  
  /* All the free linked list bookkeeping is done inside coalesce */
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

  //size_t first_size; Unused.
  //size_t first_alloc; Unused.
  // size_t first_prev_alloc = 1; Unused.
  

  if((heap_listp = mem_sbrk(2*WSIZE)) == (void *)-1) // May only need 2*WSIZE.
    return -1;
  
  PUT(heap_listp, 0); /* Alignment padding */
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1, 1)); /* Prologue header (ignore footer)*/
  PUT(heap_listp + (3*WSIZE), PACK(0, 1, 1)); /* Epilogue header */
  
  /* Cannot Move this line anywhere else, will cause SegFault */
  heap_listp += (2*WSIZE); /* Put heap_listp right after the prologue block. */
  free_list = heap_listp; //heap_listp is the first free block.
  
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

/* find_fit revamped, only searches through the list of free blocks */
static void *find_fit_free(size_t asize)
{
  /* First fit search */
  void *bp;
  
  if(free_list == NULL) /* Can change this if we set the first "init" block to be free */
    find_fit(asize);
  else{
    for (bp = free_list; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_FREE(bp)) {
      if (asize <= GET_SIZE(HDRP(bp))) {
        
        remove_list(bp); /* Removes the block about to be allocated from the list */
        
        return bp;
      }
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

  if ((csize - asize) >= (MIN_BLOCK_SIZE)) { /* If enough extra space for second block to be made: */
    
    PUT(HDRP(bp), PACK(asize, prev_alloc, 1)); /* Make header. */
    
    PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 1, 0)); /* Write header of second block. */
    if(free_list != NULL){ /* Needed for first execution */
      
      PUTA(PREV_FREE(NEXT_BLKP(bp)), NOPREV); //There's nothing before the head of list... either that or last element of current list. Need to find a way to get it.
      PUTA(NEXT_FREE(NEXT_BLKP(bp)), free_list); /* Previous Head of list is now second element */
      PUTA(PREV_FREE(free_list), NEXT_BLKP(bp)); /* Sets the prev address to current free block */
      free_list = NEXT_BLKP(bp); /* New Head of list is block created */ //Do not know if I need the GET Macro
    }
    else{
      
      free_list = NEXT_BLKP(bp);
    }
    
    PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 1, 0)); /* Write footer of second block. */

  }
  else {  /* Make Header And Inform next block I'm allocated. */
    PUT(HDRP(bp), PACK(csize, prev_alloc, 1));
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
  if(size == 0)
    return NULL;
  
  /* Adjust block size to include overhead and alignment reqs. */
    asize = ALIGN(4 + size);

  /* Search the free list for a fit */
  if ((bp = find_fit_free(asize)) != NULL) {
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

// if(free_list != NEXT_BLKP(bp)){ /* Remove the next block from the list, then coalesce */
//     remove(NEXT_BLKP(bp)); /* Removes next block from the free list */
//     PUT(NEXT_FREE(bp), free_list); /* Sets a new Head for the list, puts the prev head as second element. */
//     PUT(PREV_FREE(bp), NOPREV); /* There's no previous for the beginning of the list */
//     PUT(PREV_FREE(free_list), bp); /* Sets previous free from second element to Current block (First element) */
//     free_list = bp; /* Changes the head of the list to this bp */
//    }
//  else{

/* The next block is the head of the list, grab info and move it down one block */

//  PUT(NEXT_FREE(bp), NEXT_FREE(free_list)); /* Copy the next free block's address into the just free'd block's next  */
// PUT(PREV_FREE(bp), NOPREV); /* Set previous to none because this block is now the head of the list  */
//  PUT(PREV_FREE(NEXT_FREE(free_list)), bp); /* Set the next's block previous address value to this block's address   */
//  free_list = bp; /* make the just free'd block the new head of free list */
//    }
//    
//    PUT(FTRP(bp), PACK(size, 1, 0));
//

/* The prev block is the head of the list, do NOTHING :) */
//  if(free_list != PREV_BLKP(bp)){
//  PUT(NEXT_FREE(PREV_BLKP(bp)), free_list);
//  PUT(PREV_FREE(free_list), PREV_BLKP(bp));
// PUT(PREV_FREE(PREV_BLKP(bp)), NOPREV);
// free_list = PREV_BLKP(bp);
/* This piece of code can be "optimized" (better style) by moving bp = prev block to the top */
//  }
//    bp = PREV_BLKP(bp);





//    if((free_list != PREV_BLKP(bp)) && (free_list != NEXT_BLKP(bp))) { 
//  PUT(NEXT_FREE(PREV_BLKP(bp)), free_list);
//   PUT(PREV_FREE(free_list), PREV_BLKP(bp));
//   PUT(PREV_FREE(PREV_BLKP(bp)), NOPREV);

//   free_list = PREV_BLKP(bp);
/* This piece of code can be "optimized" (better style) by moving bp = prev block to the top */
//   }
//  else if(free_list == NEXT_BLKP(bp)){ /* Next bp is Current head of list, handle it. */

/* Harder than expected because previous block is already on the list */

//   }
//    else { /* Previous bp is Current head of list, handle it */

/* Harder than expected because next block is already on the list */

//  }
//    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1, 0));
