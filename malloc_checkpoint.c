/* 
 * Name : Kavya Srinet
 * andrew id: ksrinet
 * 
 * The mm.c file here implements an allocator using explicit free lists
 * with boundary tag and coalescing.
 * The block design is as follows :
 * Each block has a header and a footer, where they are of the form :
 * 31                          3  2  1  0 
 * --------------------------------------
 *|s  s  s  .....  . s  s  s s  0  0  a/f |
 * -------------------------------------- 
 * where s bits together indicate the size of the block and the a/f bit
 * indicate whether the block is free or allocated
 * For each allocated block, we have a header and footer of 4 bytes in the
 * aforementioned format.
 *
 * For each free block, we have a header and footer of 4 byte each
 * and a pointer to the next and previous free blocks (8 byte each for 64 bit system)
 * 
 * The heap itself has a pointer to the head of the free list, followed by 
 * a pointer to the tail of the free list, to keep a track of the list of free blocks
 * The heap begins with a padding of 4 bytes, followed by prologue header and footer
 * of 4 byte each, size 8 and a/f bit set, followed by user blocks, at the end is an epilogue block
 * of 4 bytes, size zero and a/f bit set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "mm.h"
#include "memlib.h"

#define DEBUG1x

#ifdef DEBUG1
# define dbg1(...) printf(__VA_ARGS__)
#else
# define dbg1(...)
#endif

/* Logging Functions
 * -----------------
 * -checkheap acts like mm_checkheap, but prints the line it failed on and
 *    exits if it fails.
 */

#ifndef NDEBUG
#define checkheap(verbose) do {if (mm_checkheap(verbose)){\
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);\
                        }}while(0)
#else
#define checkheap(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8      /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<9)  /*initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */
#define HEADER      4      /*Header size in bytes*/
#define FOOTER      4      /*Footer size in bytes*/

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((unsigned)((size) | (alloc)))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))
#define GET_8_byte(p)      (*(unsigned long *)(p)) /*As pointers in 64 bit system are 8 byte each use unsigned long*/
#define PUT_8_byte(p, val) (*(unsigned long *)(p) = (unsigned long)(val)) 

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7) /*the 29 MSB bits determine the size as per the design*/
#define GET_ALLOC(p) (GET(p) & 0x1) /*last bit is for allocated/free*/
#define PREVIOUS_ALLOC(p) (GET(p) & 0x2) /*second last bit from LSB keeps track of whether the previous block is allocated*/

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)  /*Address of header*/
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Find the maximum of two numbers */
#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Given block pointer bp, get the next and previous blocks */
#define NEXT_BLOCK(bp)  ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLOCK(bp)  ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/*For free block pointer bp, find the next free block pointed to by next field*/
#define FREE_NEXT(bp) ((char*)(bp))
/*for free block pointer bp, find previous fre block pointed to by previous field*/
#define FREE_PREV(bp) ((char*)(bp) + DSIZE)

/*For free block pointer bp, find the pointers to  next and previous free blocks */
#define FREE_NEXT_PTR(bp)  ((char*)GET_8_byte((char*)(bp)))
#define FREE_PREV_PTR(bp)  ((char*)GET_8_byte((char*)(bp) + DSIZE))

/*We keep two pointers to start and end of free lists*/
/* Get the start and end of a list of free blocks*/
#define HEAD_FREE ((char *)heap_head)
#define TAIL_FREE ((char *)HEAD_FREE + DSIZE)
/*Get the pointer to start and end of free list*/
#define HEAD_FREE_PTR ((void *)GET_8_byte(HEAD_FREE))
#define TAIL_FREE_PTR ((void *)GET_8_byte(TAIL_FREE))

/*Global variables*/
static char *heap_ptr;         /* Pointer to the heap*/
static char *heap_head;       /* Head of the heap and head of free list */
void *epilogue_ptr;                /* Point to the epilogue block as it keeps shifting */

/* Function prototypes for internal helper functions */
static void add_free_blk(void* bp);
static void rem_free_blk(void* bp);
void *find_fit(size_t);
void *extend_heap(size_t);
void *coalesce(void *);
void place(void*, size_t);
static void check_block(void *bp);
static void print_block(void *bp);
void print_list(void);
//static void _checkheap(void);

static int verbose;

/* 
 * mm_init - Called when a new trace starts 
 * and initializes the heap allocation
 */
int mm_init(void){
	dbg1("Inside the function mm_init()\n");

	/* Create the initial empty heap starting with two pointers to free lists and padding block, prologue 
	 * header and footer and then the epilogue block
	 */
	if((heap_ptr = mem_sbrk(2*DSIZE + 4*WSIZE)) == NULL){/*If sbrk funcion has an error, return -1*/
		return -1;
	}
	heap_head = heap_ptr;                          /*Keep the head of the heap to assign it to free list head later*/
	PUT_8_byte(heap_ptr, NULL);                    /*Pointer to head of the free list*/
	PUT_8_byte(heap_ptr + DSIZE, NULL);	       /*Pointer to end of free list*/
	heap_ptr = heap_ptr + 2*DSIZE; 	               /*start the heap after the two pointers*/
	PUT(heap_ptr, 0);                              /*Padding for alignment to 8 bytes*/
	PUT(heap_ptr + WSIZE, PACK(OVERHEAD, 1));      /*Prologue header of 4 bytes that has the size and a/f bit as 1 */
	PUT(heap_ptr + (2 * WSIZE), PACK(OVERHEAD, 1));/*Prologue footer follows and is identical to header */ 
	PUT(heap_ptr + (3 * WSIZE), PACK(0, 1));       /*At the end is the Epilogue header with 0 size and a/f set to 1 */
	epilogue_ptr = (unsigned *)(heap_ptr+3*WSIZE); /*Save the pointer to epilogue block as this will keep shifting as we extend the heap*/

	/* Move the heap pointer next to prologue header*/
	heap_ptr += DSIZE;

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL){ /*If extend_heap throws an error, return -1*/
		return -1;
	}

	dbg1("Succesfully initiated the heap allocation, stepping outside mm_init()\n");

	return 0; /*Succesful allocations*/
}

/* 
 * malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 * Allocate a block of size equal to at least the requested size
 */
void *malloc(size_t size){
	dbg1("Inside function malloc(%ld)\n",size);
	/*local variables*/   
	size_t asize;      /* Adjusted block size*/
	size_t extendsize; /* Amount to extend heap if no fit*/
	char *bp;          /* Points to first byte of payload in a block*/
 
	checkheap(0); /*Check if the heap is alright*/

	/* Ignore spurious requests */
	if (size <= 0){
		return NULL;/*not valid size*/
	}
	/*Adjust block to include overhead and alignment reqs.*/
	else if (size <= (2*DSIZE)){
		asize = (2*DSIZE + OVERHEAD); /*Size plus header and footer*/
	}
	else{
	/*if requested size greater than 2^4 allocate the next aligned multiple of 8*/
	asize = (2*DSIZE) * ((size + (OVERHEAD) + (DSIZE)+ (DSIZE-1)) / (2*DSIZE));
	}
  
	/* Search the free list for a fit and place it if found*/
	if((bp = find_fit(asize)) != NULL){ /*If a fit is found, place it*/
		place(bp, asize);
		dbg1("found a fitin free list in malloc()\n");
		return bp;
	} 
	/* No fit found. Get more memory and place the block. */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL){ /*If no heap space available*/
		return NULL;
	}
	place(bp, asize);/*place the block*/
	dbg1("Exiting malloc by extending the heap\n");
  
	return bp;
}

/*
 * calloc - Allocate the block and initialize it to zero.
 * we allocate (nmemb * size) bytes
 * 
 */
void *calloc(size_t nmemb, size_t size){
	dbg1("Inside the function calloc()\n");
	/*local variables*/
	char *ptr;
	int total_size;

 	total_size = (nmemb * size); /*Total size needed to be allocated*/
	ptr = (char *) malloc(total_size); /*Call malloc() to allocate space and get the pointer to first byte*/
	
	/* After the allcattion, initialize each byte to zero, similar to abzero function */
	memset(ptr, 0 , total_size);

	dbg1("Exiting calloc()\n");
	return ptr;
}

/*
 * free - Free a given block and coalesce it
 * with an adjacent free block if any
 */
void free(void *ptr){
	dbg1("Inside function free(%p)\n", ptr);
	size_t size;
	if(ptr == NULL) {
		dbg1("pointer null , exiting free()\n");
		return;
	}

	size = GET_SIZE(HDRP(ptr));

	/* Set the a/f bit of header and footer to zero */
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	/*Now coalesce this free block with any adjacent free block*/
	coalesce(ptr);

	dbg1("Succesfully exiting free()\n");  
}

/* realloc - Reallocates memory according to specific size
 * Change the size of the block by mallocing a new block and 
 * freeing the old one
 */
void *realloc(void *oldptr, size_t size){
	dbg1("Inside relloc()\n");
	/*local variables*/
	size_t oldsize; /* Hold the size of the block pointed to by oldptr  */
	char *newptr;   /* Points to the newly alllocated block */

	if(size == 0){ /*If a size of 0 has to be allocated, just free the oldptr*/
		free(oldptr);
		return NULL;
	}
	
	/*If oldptr is NULL, call a simple malloc for size then*/
	if(oldptr == NULL){
		return malloc(size);
	}
	/* Calculate payload bytes contained in oldptr */
	oldsize = GET_SIZE(HDRP(oldptr)) - OVERHEAD;
  	/* If requested size is smaller than old one, old ptr will suffice, so return */
	if(size <= oldsize){
		return oldptr;
	}
	/* Otherwise, allocated a fresh new block, copy previous bytes and delete the oldptr Need more memory */
	newptr = (char *) malloc(size);
	if(!newptr){
		dbg1("malloc error in realloc()");
		return NULL;
	}
	memcpy(newptr, oldptr, size);
	free(oldptr);

	dbg1("Exiting realloc()succesfully\n");
	return newptr;
}


/* The remaining routines are internal helper routines */

/*
 * Insert a freshly freed block into the list of free
 * blocks, as of now we have implemented FIFO and insert
 * the new block at the end of all free blocks in the list
 */

static void add_free_blk(void* bp){
	dbg1("Inside  add_free_blk()\n");

	/* if the list is empty,set the head_free to point to this block*/
	if(HEAD_FREE_PTR == NULL){
		PUT_8_byte(HEAD_FREE, bp);
		PUT_8_byte(FREE_PREV(bp), NULL); /*Set its prev pointer as null*/
	}
	else{
		/* if there are free blocks, add this one at the end*/
		PUT_8_byte(FREE_PREV(bp), TAIL_FREE_PTR); /*prev ptr of this is the old tail ptr*/
		PUT_8_byte(FREE_NEXT(TAIL_FREE_PTR), bp); /*set next ptr of old tail ptr to this one*/
	}

	/*Set the next ptr of this block to NULL so this is the last block in the list */
	PUT_8_byte(FREE_NEXT(bp), NULL);
	PUT_8_byte(TAIL_FREE, bp);/*New tail ptr is this block*/

	dbg1("Exiting the add_free_blk() function successfully\n");

        return;
}

/*
 * rem_free_blk- Remove the free block that
 * bp points to and update the prev and next ptrs
 * of neighboring blocks in the list. There can be four cases
 * the block can either be the first, last or middle block or
 * the only block in the list
 */
static void rem_free_blk(void *bp){

	dbg1("Inside function rem_free_blk()\n");

	/*If bp points to the only block, mak ethe list empty*/
	if((((void*)FREE_PREV_PTR(bp) == NULL)) && ((void*)FREE_NEXT_PTR(bp) == NULL)){
		PUT_8_byte(HEAD_FREE, NULL); /*make the list null*/
		PUT_8_byte(TAIL_FREE, NULL);
		dbg1("Made the list empty, exiting rem_free_blk()\n");
		return;
	}
	else if((void*)FREE_PREV_PTR(bp) == NULL){ /*If first block in the list, remove this and update the list*/
		PUT_8_byte(FREE_PREV(FREE_NEXT_PTR(bp)), NULL); /*set next block as the first block in the list*/
		PUT_8_byte(HEAD_FREE, FREE_NEXT_PTR(bp)); /*head now points to this*/
	}
	else if((void*)FREE_NEXT_PTR(bp) == NULL){ /*If last block in the list, remove and update list*/
		PUT_8_byte(FREE_NEXT(FREE_PREV_PTR(bp)), NULL); /*set the previous block as last block in list*/
		PUT_8_byte(TAIL_FREE, FREE_PREV_PTR(bp)); /*tail now points to this block*/
	}
	else{ /*The block is in the middle, remove it and adjust prev and next blocks*/
		PUT_8_byte(FREE_PREV(FREE_NEXT_PTR(bp)), FREE_PREV_PTR(bp));/*adjust prev ptr of next block to prev block*/
		PUT_8_byte(FREE_NEXT(FREE_PREV_PTR(bp)), FREE_NEXT_PTR(bp)); /*adjust next ptr of prev block to next blk*/
 	}
        dbg1("Exiting rem_free_blk()\n");
}

/*
 * find_fit - Find a fit for a block with asize bytes
 * from the list of free blocks we have maintained
 * we have implemented first fit as of now
 */
void *find_fit(size_t asize){
        dbg1("Inside find_fit()\n");
        void* bp; /*Hold the pointer to current free block*/

        /* First fit search */
        /*iterate from head of free lists till ptr is null, to get a block that fits*/
        for(bp = (void *)HEAD_FREE_PTR; bp != NULL; bp = (void *)FREE_NEXT_PTR(bp)){
                if(asize <= GET_SIZE(HDRP(bp))){ /*if the requested block fits in this, return ptr to this block*/
                        dbg1("Found a fit , returning from find_fit()\n");
                        return bp;
                }
        }
        /*otherwise, no fit found*/
        dbg1("did not find a fit, returning from find_fit()\n");
        return NULL;
}

/* 
 * extend_heap - Extend heap with free blocks of words
 * and return a pointer to it
 */
void *extend_heap(size_t words){
	dbg1("Inside extend_heap()\n");
	char *bp;     /* A pointer to extended heap*/
	size_t size;  /* Requested size*/
	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if((long)(bp = mem_sbrk(size)) < 0) {/*if sbrk error, return NULL*/
		dbg1("Exiting extend_heap() sbrk error\n");
		return NULL;
	}

	/* Initialize the free block header, footer and new epilogue header*/
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLOCK(bp)), PACK(0, 1));

	epilogue_ptr = (unsigned *)HDRP(NEXT_BLOCK(bp)); /*update the pointer to new epilogue block*/

	dbg1("Succesfully exiting from extend_heap()\n");
 	return coalesce(bp);/*coalesce if any adjacent free block*/
}


/*
 * coalesce - perform boundary tag coalescing.
 * Merge any two adjacent free blocks and 
 * return a pointer  to coalesced block
 */
void *coalesce(void *bp){
	dbg1("Inside coalesce()\n");
	size_t size = GET_SIZE(HDRP(bp));
	size_t next_blk_a = GET_ALLOC(HDRP(NEXT_BLOCK(bp))); /*check if next block is allocated*/
	size_t prev_blk_a = (GET_ALLOC((unsigned char *)HDRP(bp) - FOOTER) << 2); /*check if prev block is allocated and shift by 2*/
	int check;

	check = (prev_blk_a | next_blk_a);

	switch(check){
	 case 0:
                 /*if both next and previous are free*/
                size += GET_SIZE(HDRP(PREV_BLOCK(bp))) + GET_SIZE(FTRP(NEXT_BLOCK(bp)));/*size is size plus size of prev and next blocks*/
		rem_free_blk(NEXT_BLOCK(bp));/*update free list and remove prev and next blocks from it*/
                rem_free_blk(PREV_BLOCK(bp));
                PUT(HDRP(PREV_BLOCK(bp)), PACK(size, 0));/*coalesce with prev and next blocks*/
                PUT(FTRP(NEXT_BLOCK(bp)), PACK(size, 0));
                add_free_blk(PREV_BLOCK(bp)); /*Now add this big new block to free list*/
                bp = PREV_BLOCK(bp); /*update pointer*/
                break;
	case 1: /*If previous block is free but next block is not*/
		size += GET_SIZE(HDRP(PREV_BLOCK(bp)));/*size is size plus previous block's size*/
		rem_free_blk(PREV_BLOCK(bp));
		PUT(HDRP(PREV_BLOCK(bp)), PACK(size, 0));/*coalesce with prev block*/
		PUT(FTRP(bp), PACK(size, 0));
		rem_free_blk(PREV_BLOCK(bp));/*remove previosu block from free list*/
		add_free_blk(PREV_BLOCK(bp)); /*add this new block to free list*/
		bp = PREV_BLOCK(bp); /*update pointer*/
                break;
	case 4: /*If next block is free but previous block is not*/
		size += GET_SIZE(HDRP(NEXT_BLOCK(bp)));/*size is size plus next block's size*/
		rem_free_blk(NEXT_BLOCK(bp));
		PUT(HDRP(bp), PACK(size, 0));/*coalesce with next block*/
		PUT(FTRP(bp), PACK(size, 0));		
		add_free_blk(bp);
		break;
	case 5: /*If both prev and next are allocated*/
		add_free_blk(bp);
	}
	return (bp);
}

/*
 * place - Place block of asize bytes at the block
 * pointed to by bp and split if the remainder >= minimum
 * block size
 */
void place(void *bp, size_t asize){
        dbg1("Inside place()\n");
        size_t blk_size = GET_SIZE(HDRP(bp));

        /*Remmove this block from free list and check for splitting */
        rem_free_blk(bp);
        if (blk_size - asize < OVERHEAD + 2*DSIZE){ /*if remainder is < minimum block size, allocate the whole block*/
                PUT(HDRP(bp), PACK(blk_size, 1));/*allocate the whole block, set header and footer*/
                PUT(FTRP(bp), PACK(blk_size, 1));
        }
        else{ /*otherwise remiander >= min block size*/
                asize = (asize + 0x7) & ~0x7; /*align the size*/
                PUT(HDRP(bp), PACK(asize, 1)); /*allocate a block of this size, set header and footer*/
                PUT(FTRP(bp), PACK(asize, 1));
                PUT(HDRP(NEXT_BLOCK(bp)), PACK(blk_size - asize, 0)); /*split the block and set the remainder free*/
                PUT(FTRP(NEXT_BLOCK(bp)), PACK(blk_size - asize, 0));
                add_free_blk((void*)NEXT_BLOCK(bp)); /*add the split block to the pool of free blocks*/
        }

        dbg1("Exiting the function place()\n");
}

/*
 *  * mm_checkheap - Performs various sanity checks on heap
 *   *                Adapted from CS:APP version to work with explicit lists
 *    */
 

static void print_block(void *bp){
  size_t hsize, halloc, hpalloc, fsize, falloc, fpalloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
	hpalloc = PREVIOUS_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));
	fpalloc = PREVIOUS_ALLOC(HDRP(bp));

  if (hsize == 0) {
    printf("%p: EOL (size=0): header: [%ld:%c:%c]\n", bp,
      hsize, (hpalloc ? 'a' : 'f'), (halloc ? 'a' : 'f'));
    return;
  }

	printf("%p: header: [%ld:%c:%c] footer: [%ld:%c:%c]\n", bp,
			hsize, (hpalloc ? 'a' : 'f'), (halloc ? 'a' : 'f'),
			fsize, (fpalloc ? 'a' : 'f'), (falloc ? 'a' : 'f'));
}

static void check_block(void *bp){
  size_t halloc = GET_ALLOC(HDRP(bp));

	/* check if free block is inside heap */
	if ((bp < mem_heap_lo()) || (mem_heap_hi() < bp)) {
		printf("mm_check: block pointer %p outside of heap\n",bp);
		fflush(stdout);
		exit(0);
	}

	/* alignment check */
  if ((size_t)bp % 8) {
    printf("Error: %p is not doubleword aligned\n", bp);
		exit(0);
	}

  /* allocated block does not have footer */
	if (!halloc) {
		if (GET(HDRP(bp)) != GET(FTRP(bp))) {
			printf("Error: header does not match footer\n");
			exit(0);
		}
	}
}


/*
 *  *  checkheap - Check the heap for consistency
 *   *  (iterates all the blocks starting from prologue to epilogue)
 *    *  
 *     */
int mm_checkheap(int v){
	verbose  = v;
	char *bp = heap_ptr;
	size_t prev_alloc;//,curr_alloc;

	dbg1("\n[verbose:%d]\n", verbose);
	dbg1("\n[CHECK HEAP]\n");

	if (verbose) {
		printf("Heap (starting address:%p):\n", heap_ptr);
		printf("-prologue-");
		print_block(bp);
	}

		 /* checking prologue block (size, allocate bit)*/
		  	if ((GET_SIZE(HDRP(heap_ptr)) != DSIZE) || !GET_ALLOC(HDRP(heap_ptr))) {
		 			printf("Bad prologue header\n");
		  					printf("-prologue-");
		  							print_block(bp);
					return 1;
		  								}
							check_block(heap_ptr); // alignment, header/footer
						prev_alloc = GET_ALLOC(HDRP(bp));
		 
/*
	 // checking allocated/free blocks
	for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		curr_alloc = GET_PREV_ALLOC(HDRP(bp));
		if (verbose)
			printblock(bp);
		if (!prev_alloc != !curr_alloc) {
		// previous block's allocate bit should match current block's prev allocate bit
		printf("prev allocate bit mismatch\n");
		exit(0);
		}
		prev_alloc = GET_ALLOC(HDRP(bp));
		checkblock(bp);
 	}*
// checking epilouge block
	if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))){
		printf("Bad epilogue header\n");
		printf("-epilogue-");
		printblock(bp);
		return 1;
	}*/
	dbg1("[CHECK DONE]\n\n");
	return 0;
}

void print_list(void){
	void* bp = HEAD_FREE_PTR;
	for (;bp != NULL;bp = FREE_NEXT_PTR(bp))
		print_block(bp);
}

