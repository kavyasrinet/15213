/*
 * Name : Kavya Srinet
 * andrew_id : ksrinet
 *
 * The mm.c file here implements an allocator using segregated free lists
 * with boundary tag and coalescing.
 * The block design is as follows :
 * Each free block has a eader and a footer and each allocated block has a header.
 * the design of header and footer is as follows:
 * 31                          3  2  1  0
 * --------------------------------------
 * s  s  s  .....  . s  s  s s  s  0 pa  a/f 
 * --------------------------------------
 * where s bits together indicate the size of the block, pa bit indicates if 
 * the previous block is allocated or not, and the a/f bit indicate whether the block is free or allocated
 * For each free block, we we have a header and footer of 4 byte each
 * and a pointer to the next and previous free blocks (8 byte each for 64 bit system)
 * For each allocated block, we have a header and payload.
 *
 * The heap starts with pointers to heads of each segregated lists, followed by a padding of 4 bytes,
 * then prologue block, allocated blocks and then the epilogue block
 *
 * For the implementation here, the minimum block size os 24 bytes, each chunk is 168 bytes
 * and each of the segregated lists is a power of two, in increasing order, we have 12 total lists
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 *  Logging Functions
 *  -----------------
 *  - dbg_printf acts like printf, but will not be run in a release build.
 *  - checkheap acts like mm_checkheap, but prints the line it failed on and
 *  exits if it fails.
 */

#ifndef NDEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define checkheap(verbose) do {if (mm_checkheap(verbose)) {  \
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);  \
                        }}while(0)
#else
#define dbg_printf(...)
#define checkheap(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif				/* def DRIVER */



/* define global constants */
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (168) /*extend heap by this amount in bytes*/
#define OVERHEAD    8 /*header plus footer*/
#define MIN_BLOCK_SIZE    24 /*minimum block size is header plus footer plus pointers to next and previous free blocks*/

/* Function macros */
#define MAX(x, y)   ((x) > (y) ? (x) : (y)) /*return the maximum of two entities*/
#define MIN(x, y)   ((x) < (y) ? (x) : (y)) /*return minimum of two entities*/

/* Pack a size and allocated bit into one single word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write from/to address p */
#define GET(p)      (*((size_t*) (p))) /*Read 8 bytes from address p*/
#define PUT(p, val) (*((size_t*) (p)) = (val)) /*place val at the address p*/

/* Read and write from/to address p */
#define GET_4(p)     (*((unsigned*) (p))) /*Read 4 bytes from p address*/
#define PUT_4(p, val)(*((unsigned*) (p)) = (val)) /*Write val to address p*/

/* For a block ptr bp, get address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/* For a block pointer bp, get address of its header and footer */
#define HDRP(bp)    ((char*)(bp) - WSIZE) /*As block pointer points to right after header*/
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)         (GET_4(p) & ~0x7) /*size is all 4 bytes in p except the 3 lsb bits*/
#define GET_PREV_ALLOC(p)   (GET_4(p) & 0x2) /*prev allocated bit is 2nd lsb bit, indicating if previous block is allocated*/
#define GET_ALLOC(p)        (GET_4(p) & 0x1) /*a/f bit is the lsb bit*/

/* For a block ptr bp, which points to a free block 
 * get pointer to next and previous free blocks
 */
#define NEXT_FREE(bp)   ((char*) ((char*)(bp)))
#define PREV_FREE(bp)   ((char*) ((char*)(bp) + DSIZE))

/* We use the lower 3 bits in the  header to store info about
 * whether or not the block is allocated and previous
 * block is allocated
 */
#define ALLOC   0x01
#define PREV_ALLOC  0x02

/* Total number of segregated lists */
#define NO_OF_LISTS   12

/* Maximum size limit of each list in powers of two*/
#define LIST0_SIZE      128
#define LIST1_SIZE      256
#define LIST2_SIZE      512
#define LIST3_SIZE      1024
#define LIST4_SIZE      2048
#define LIST5_SIZE      4096
#define LIST6_SIZE      8192
#define LIST7_SIZE      16834
#define LIST8_SIZE      32768
#define LIST9_SIZE      65536
#define LIST10_SIZE     131072

/*
 * Offsets of segregated lists, as each of them
 * is a pointer to that list's header, each is 8 byte long
 */
#define LIST_0     0
#define LIST_1     8
#define LIST_2     16
#define LIST_3     24
#define LIST_4     32
#define LIST_5     40
#define LIST_6     48
#define LIST_7     56
#define LIST_8     64
#define LIST_9     72
#define LIST_10    80
#define LIST_11    88

/* Helper functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_block_in_list(size_t index, size_t size);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_free_blk(char *bp, size_t size);
static void rem_free_blk(char *bp, size_t size);
static size_t get_index(size_t asize);
static size_t find_list(size_t ind);
static size_t find_list_by_size(size_t ind);
static size_t get_list_size(size_t index);
static int  check_free_blk(char *blk);
static int  check_alloc_blk(char *blk);
static void  print_free_blk(char *blk);
static void  print_alloc_blk(char *blk);
static int check_seg_lists();
static int check_for_cycle();
static int check_free_blk_count();

/* Global variables-- Base of the heap(heap_listp) */
static char *heap_ptr;
char *heap_start;


/* 
 * mm_init - This function initializes the heap
 * We keep the pointers to the segregated lists first 
 * followed by the alignment padding , prologue and eplilogue blocks.
 * We extend the heap by CHUNKSIZE bytes
 */
int mm_init(void){

	/* we start with allocating pointers (8 bytes) to each segregated list */
	if ((heap_ptr = mem_sbrk(NO_OF_LISTS * DSIZE)) == NULL){
        	return -1; /*if sbrk error*/
	}

	 /* Now we initialize each of these pointers as NULL */
	PUT(heap_ptr + LIST_0, (size_t) NULL);
	PUT(heap_ptr + LIST_1, (size_t) NULL);
	PUT(heap_ptr + LIST_2, (size_t) NULL);
	PUT(heap_ptr + LIST_3, (size_t) NULL);
	PUT(heap_ptr + LIST_4, (size_t) NULL);
	PUT(heap_ptr + LIST_5, (size_t) NULL);
	PUT(heap_ptr + LIST_6, (size_t) NULL);
	PUT(heap_ptr + LIST_7, (size_t) NULL);
	PUT(heap_ptr + LIST_8, (size_t) NULL);
	PUT(heap_ptr + LIST_9, (size_t) NULL);
	PUT(heap_ptr + LIST_10, (size_t) NULL);
	PUT(heap_ptr + LIST_11, (size_t) NULL);

	/*Now we allocate space for padding, prologue block and epilogue*/
	if ((heap_start = mem_sbrk(4 * WSIZE)) == NULL){ /*we need 4*WSIZE space as each of pad, prologue header and footer and epilogue are 4 byte each*/
		return -1;
	}
	PUT_4(heap_start, 0); /*Alignment padding*/
	PUT_4(heap_start + WSIZE, PACK(DSIZE, 1)); /*Prologue header*/
	PUT_4(heap_start + (2 * WSIZE), PACK(DSIZE, 1)); /*Prologue footer*/
	PUT_4(heap_start + (3 * WSIZE), PACK(0, PREV_ALLOC | ALLOC)); /*Epilogue header with information about previosu allocated block*/
	/*Create an empty space of CHUNKSIZE bytes */
	heap_start = heap_start + DSIZE;
	if (extend_heap(CHUNKSIZE) == NULL){
	        return -1;
	}
	return 0; /*successful exit*/
}

/*
 *  Malloc - Allocates at least size number of bytes on the heap
 *  and returns a pointer to the payload of block
 *  We first see if we have any free list of size >= required size
 *  if not available, we extend the heap
 */
void *malloc(size_t size){

	size_t asize;           /* adjusted and aligned size */
	size_t extendsize;          /* need to extend by this size */
	char *bp;   /*pointer to block to be returned*/
	
	/* Ignore spurious requests */
	if (size <= 0){
		return NULL;
	}
	/* Adjust the block size and include overhead of header and alignment */
	if (size <= 2 * DSIZE){
		asize = MIN_BLOCK_SIZE; /*Smallest aligned block*/
	}
	else{
	/* Otherwise round up to next multiple of 8*/
	asize = (((size_t) (size + WSIZE) + 7) & ~0x7);
	}
	/* See if any free lists are availbale that can fit the requested size */
	if ((bp = find_fit(asize)) != NULL) {
        	place(bp, asize);       /* Block found, place it */
        	return bp; /*return pointer to this block*/
	}
	/* Otherwise extend the heap with the max of aligned size or CHUNKSIZE */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize)) == NULL){
		return NULL; /*Cannot extend more*/
	}
	place(bp, asize);           /* Place the block */
	return bp;
}

/*
 *  free - This function frees the block pointed to by bp
 *  and adds this block to the proper segreagated list of free blocks
 */
void free(void *bp){
	size_t size; /*size of block pointed to by bp*/
	char *header_next; /*pointer to header of next block*/

	if (bp == NULL){
		return;
	}
	header_next = HDRP(NEXT_BLKP(bp)); /*pointer to next block*/
	size = GET_SIZE(HDRP(bp)); /*get size of this block*/
	PUT_4(header_next, (GET_SIZE(header_next) | 0 | GET_ALLOC(header_next))); /*update previous blk allocated bit to 0, for next block*/

	PUT_4(HDRP(bp), (size | GET_PREV_ALLOC(HDRP(bp)) | 0));/*Set allocated bit to 0, and retain everything else, for header*/
	PUT_4(FTRP(bp), GET_4(HDRP(bp))); /*Footer is a replica of header*/

	add_free_blk(bp, size); /*Add this block to the correct seg list*/
	coalesce(bp); /*coalesce if possible*/
}

/*
 *  realloc - Reallocates memory according to specific size
 *  Change the size of the block by mallocing a new block and
 *  freeing the old one
 */
void *realloc(void *oldptr, size_t size){

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
                return NULL;
        }
        memcpy(newptr, oldptr, size);
        free(oldptr);
        return newptr;
}

/*
 * calloc - Allocates memory for an array of nmemb elements
 * of size bytes each and returns a pointer to the allocated
 * memory. The memory is set to zero before returning.
 */
void *calloc(size_t nmemb, size_t size){
	char *ptr, *temp;
        size_t total_size, i;

        total_size = (nmemb * size); /*Total size needed to be allocated*/
        ptr = malloc(total_size); /*Call malloc() to allocate space and get the pointer to first byte*/
	temp = ptr;
        /* After the allcattion, initialize each byte to zero, similar to abzero function */
	for (i = 0; i < nmemb; i++){
		*temp = 0;
		temp = (temp + size);
	}
	return ptr;
}

/*
 * mm_checkheap - This function tests the heap consistency
 * for the following conditions:
 * 1. Check if prologue header and footer are aligned, similar and in heap
 * 2. If the first block pointed to is a free block, check it
 * Free blocks are checked for :
 * 	1. If the header and footer are similar
 *      2. If the block has an adjacent free block
 *      3. If the block is aligned
 *      4. The pointed from next block is consistent
 *      5. the pointer from previous block is consistent
 *      6. If the free block is in the heap i.e > mem_heap_lo() and < mem_heap_hi()
 * 3. Now traverse the whole heap and check each free/ allocated block.
 *    Free blocks are checked as above and allocated blocks as :
 * 	1. If the block is in the heap
 *      2. If the allocatecd block is aligned (as allocated block does not have a footer
 *         we only check the block and header
 * 4. Now check the epilogue block, for size and allocated bit
 * 5. Check if there are any cycles in the segregated lists
 * 6. Check if every block is in the right segreagted list (size range)
 * 7. Check if the number of free blocks calculated by traversing block wise in heap
 *    equals the total number of free blocks traversed by pointers in each seg list
 * 
 * Return 0 for no errors and 1 for an error
 */
int mm_checkheap(int verbose){
        char *blk; /*pointer to first block in heap*/

        if (verbose){
                dbg_printf("Heap (%p):\n", heap_ptr);
        }
        /*check prologue header*/
        if ((GET_SIZE(HDRP(heap_start)) != DSIZE) || !GET_ALLOC(HDRP(heap_start))){
                printf("ERROR: Bad prologue header\n");
                return 1;
        }
        /*Check the prologue header block*/
        check_alloc_blk(heap_start);
        /*check prologue footer*/
        if ((GET_SIZE(heap_start) != DSIZE) || !GET_ALLOC(heap_start)){
                printf("ERROR: Bad prologue footer\n");
                return 1;
        }
        /*Traverse through blocks in heap*/
        blk = (NO_OF_LISTS * DSIZE) + (2 * DSIZE) + (heap_ptr);/*pointer to first block in heap*/
	if (!GET_ALLOC(HDRP(blk))){ /*if blk points to a free block*/
                if(check_free_blk(blk)) /*check the free block against the aforementioned criteria and if erro, return 1*/
                        return 1;
        }

        /*Now check the complete heap*/
        while((GET_4(HDRP(blk)) != 1) && (GET_4(HDRP(blk)) != 3)){ /*while we have not reached the epilogue block*/
                if(!GET_ALLOC(HDRP(blk))){
                        if (check_free_blk(blk)) /*check free block*/
                                return 1;
                }
                else{
                        if(check_alloc_blk(blk)) /*check allocated block against aforementioned conditions*/
                                return 1;
                }
               if(verbose){ /*if verbose is set, print out the details*/
                        if(!GET_ALLOC(HDRP(blk))) /*if free blk, print info of the block*/
                                print_free_blk(blk);
                        else
                                print_alloc_blk(blk);
                }
                blk = NEXT_BLKP(blk); /*move to next pointer*/
        }

        /*check the epilogue block for size and allocation bit*/
        if((GET_SIZE(HDRP(blk)) != 0) || !(GET_ALLOC(HDRP(blk)))){
                printf("Bad epilogue header");
                return 1;
        }

        if(check_for_cycle()) /*if any cycles in any of the lists, return 1*/
                return 1;
        if(check_seg_lists()) /*If any block is improperly placed in a list, return 1*/
                return 1;
        if(check_free_blk_count()) /*check number of free block counts by traversing blockwise and pointer wise*/
                return 1;
        return 0; /*return 0, if no error*/

}


/*Helper functions*/

/*
 * extend_heap - Thsi function extends the heap by words no of bytes
 * and adds the block to the correct seg list, coalesces it 
 * and returns a pointer to it
 */
static void *extend_heap(size_t words){
	char *bp; /*pointer to the extended block*/

	if ((long) (bp = mem_sbrk(words)) < 0){
		return NULL;
	}
	PUT_4(HDRP(bp), PACK(words,(0 | GET_PREV_ALLOC(HDRP(bp))) | 0)); /*Set header	bits to retainf prev allocated status*/
	PUT_4(FTRP(bp), GET_4(HDRP(bp))); /*Footer is a replica of header*/
	PUT_4(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /*New epilogue block*/
	add_free_blk(bp, words); /*Add it to the corect segregated list*/
	return coalesce(bp); /*coalesce if possible and return a block pointer*/
}

/*
 * coalesce- This function combines adjacent free blocks
 * to form a larger free block and minimize external fragmentation
 */
static void *coalesce(void *bp){
	size_t size = GET_SIZE(HDRP(bp)); /*size of current block*/
	int check;
	char *prev_blk = PREV_BLKP(bp);
	char *prev_hdr = HDRP(prev_blk);
	size_t prev_size;
	char *next_blk = NEXT_BLKP(bp);
	size_t next_size = GET_SIZE(HDRP(next_blk));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));/*next block allocated */
	size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp)); /*previous block allocated*/
	check = (prev_alloc | next_alloc);

	switch(check){
	case 3:
		/*If both next and prev blocks are allocated return */
		break;
	case 2:
		/*If prev block is allocated and next is free */
		rem_free_blk(bp, size); /*remove both the blocks from free list*/
		rem_free_blk(next_blk, next_size);
		size += next_size; /*update the size*/
		PUT_4(HDRP(bp), PACK(size, prev_alloc));/*update the header and footer with new size and prev blk allocated*/
		PUT_4(FTRP(bp), PACK(size, prev_alloc));
		add_free_blk(bp, size);/*add this new blk to free list*/
		break;
	case 1:
		/*prev blok is allocated and next is free*/
		prev_size = GET_SIZE(prev_hdr);
		rem_free_blk(bp, size);/*remove prev blk and current blk from apt free list*/
		rem_free_blk(prev_blk, prev_size);
		size += prev_size;/*update size*/
		PUT_4(prev_hdr, PACK(size, GET_PREV_ALLOC(prev_hdr)));/*update header and footer of the free blk*/
		PUT_4(FTRP(prev_blk), GET_4(prev_hdr));
		add_free_blk(prev_blk, size);/*add it to the apt free list*/
		bp = prev_blk;/*point to prev blk*/
		break;
	case 0:
		/*Previous block is free and next block is free*/
		prev_size = GET_SIZE(prev_hdr);
		rem_free_blk(bp, size);/*remove prev, current and next free blks from the apt lists*/
        	rem_free_blk(prev_blk, prev_size);
        	rem_free_blk(next_blk, next_size);
		size += prev_size + next_size; /*update size*/
		PUT_4(prev_hdr, PACK(size, GET_PREV_ALLOC(prev_hdr)));
		PUT_4(FTRP(prev_blk), GET_4(prev_hdr));
		add_free_blk(prev_blk, size);/*add to apt list*/
		bp = prev_blk; /*point to prev blk*/
	}
	return bp; /*return pointer to the new free blk*/
}

/*
 * place - This function places the block pointed to by bp , in a block of size asize
 * if the remaining size is >= minimum block size, we split and allocate the first half
 * the second half is added to the free list as another free block
 * if not 
 * allocate the whole block
 */
static void place(void *bp, size_t asize){

	size_t blk_size = GET_SIZE(HDRP(bp));/*size of current block*/
	size_t extra = blk_size - asize; /*remaining number of bytes*/
	char *next_blk = NEXT_BLKP(bp); /*next block pointer*/

	/* Remove this block from free list and check for splitting */
	rem_free_blk(bp, blk_size);
	if (extra >= MIN_BLOCK_SIZE) {
		PUT_4(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | ALLOC)); /*allocate for previosu block*/
		next_blk = NEXT_BLKP(bp);
		PUT_4(HDRP(next_blk), extra | PREV_ALLOC);/*update header and footer of next block, as next is free*/
        	PUT_4(FTRP(next_blk), extra | PREV_ALLOC);
		add_free_blk(next_blk, extra);/*add next free block to free list*/
	}
	else {
	        PUT_4(HDRP(bp), PACK(blk_size, GET_PREV_ALLOC(HDRP(bp)) | ALLOC));/*allocate the whole block*/
		PUT_4(HDRP(next_blk), GET_4(HDRP(next_blk)) | PREV_ALLOC);/*update header of next block*/
		if (!GET_ALLOC(HDRP(next_blk))){ /*if next bloxk is free, it has a footer also, update*/
			PUT_4(FTRP(next_blk), GET_4(HDRP(next_blk)));
		}
	}
}

/*
 * find_fit - this function first  finds a list and then
 * calls find_block_in_list to find the exact block
 * in which the given block of asize will fit into
 */
static void *find_fit(size_t asize){
	size_t index; /*stores the index of the appropriate list*/
	size_t i;
	char *bp = NULL;/*stores the ptr to the free block*/
	index = get_index(asize);
	for(i = index; i < NO_OF_LISTS; i++){ /*find the block in the list, or a list that has size >= asize*/
		 if ((bp = find_block_in_list(i, asize)) != NULL)
                return bp;
        }
	return bp; /*if not found, return NULL*/
}

/*
 * get_index - Given the size of a block, return the index of the 
 * segregated list it will fit into
 */
static size_t get_index(size_t asize){
	if(asize <= LIST0_SIZE)
		return 0;
	else if(asize <= LIST1_SIZE)
		return 1;
	else if(asize <= LIST2_SIZE)
		return 2;
	else if(asize <= LIST3_SIZE)
		return 3;
	else if(asize <= LIST4_SIZE)
		return 4;
	else if(asize <= LIST5_SIZE)
		return 5;
	else if(asize <= LIST6_SIZE)
		return 6;
	else if(asize <= LIST7_SIZE)
		return 7;
	else if(asize <= LIST8_SIZE)
		return 8;
	else if(asize <= LIST9_SIZE)
		return 9;
	else if(asize <= LIST10_SIZE)
		return 10;
	else
		return 11;
}

/*
 * find_block_in_list - given the index of a list and required size to be allocated 
 * this function finds the block that can fit the required size of block in it
 */
static void *find_block_in_list(size_t index, size_t size){
	char *fit_blk = NULL; /*get the block*/
	size_t fit_list; /*the list this index represents*/
	fit_list = find_list(index); 
	fit_blk = (char *) GET(heap_ptr + fit_list);
	while (fit_blk != NULL) { /*find the right block in the list that fits*/
	        if (size <= GET_SIZE(HDRP(fit_blk))) {
	            break;
        }
        	fit_blk = (char *) GET(NEXT_FREE(fit_blk));
	}

	return fit_blk;
}

/*
 * find_list - Given the index of a list, return back 
 * the list it represenst and offset to that list
 */
static size_t find_list(size_t ind){
	if(ind == 0)
		return LIST_0;
	else if(ind == 1)
		return LIST_1;
	else if(ind == 2)
		return LIST_2;
	else if(ind == 3)
		return LIST_3;
	else if(ind == 4)
		return LIST_4;
	else if(ind == 5)
		return LIST_5;
	else if(ind == 6)
		return LIST_6;
	else if(ind == 7)
		return LIST_7;
	else if(ind == 8)
		return LIST_8;
	else if(ind == 9)
		return LIST_9;
	else if(ind == 10)
		return LIST_10;
	else
		return LIST_11;
}

/*
 * find_list_by_size - given the size of a block, 
 * find the list it belongs to and return the list's offset
 */
static size_t find_list_by_size(size_t ind){
	if(ind <= LIST0_SIZE)
		return LIST_0;
	else if (ind <= LIST1_SIZE)
		return LIST_1;
	else if(ind <= LIST2_SIZE)
		return LIST_2;
	else if(ind <= LIST3_SIZE)
		return LIST_3;
	else if(ind <= LIST4_SIZE)
		return LIST_4;
	else if(ind <= LIST5_SIZE)
		return LIST_5;
	else if(ind <= LIST6_SIZE)
		return LIST_6;
	else if(ind <= LIST7_SIZE)
		return LIST_7;
	else if(ind <= LIST8_SIZE)
		return LIST_8;
	else if(ind <= LIST9_SIZE)
		return LIST_9;
	else if(ind <= LIST10_SIZE)
		return LIST_10;
	else
		return LIST_11;
}

/*
 * add_free_blk - This function add a free block, pointed to by bp
 * , of size 'size', and place it in the correct segregated list
 */
static void add_free_blk(char *bp, size_t size){
	size_t list_num = 0;/*the list number can be added to*/  
	char *fit_blk = NULL;/*head of list*/
	char *fit_seg;
	list_num = find_list_by_size(size); /*index to list*/
	fit_seg = heap_ptr + list_num; /*get the index to the list*/
	fit_blk = (char *) GET(heap_ptr + list_num);/*get the head of list*/
	if (fit_blk != NULL){ /*If there are free blocks in the list, add this blk at the head*/
		PUT(fit_seg, (size_t) bp);
		PUT(PREV_FREE(bp), (size_t) NULL);/*set prev pointer to NULL*/
		PUT(PREV_FREE(fit_blk), (size_t) bp); /*add it at the head of the list*/
		PUT(NEXT_FREE(bp), (size_t) fit_blk);
	}
	else {/*else this is the only block in the seg list*/
		PUT(fit_seg, (size_t) bp); /*make bp the head of the list*/
		PUT(PREV_FREE(bp), (size_t) NULL);
		PUT(NEXT_FREE(bp), (size_t) NULL);
	}
}

/*
 * rem_free_blk - remove the block pointed to by bp, of size 'size'
 * from the correct segregated list, and adjust the pointers accordingly
 */
static void rem_free_blk(char *bp, size_t size){
	size_t list_num = 0; /*the list number block belongs to*/
	char *next_free_blk = (char *) GET(NEXT_FREE(bp)); /*the next free blk , bp points to*/
	char *prev_free_blk = (char *) GET(PREV_FREE(bp));/*the previous free block*/

	list_num = find_list_by_size(size); /*Get the header location of the relevant list, the block belongs to*/
	if (prev_free_blk == NULL && next_free_blk == NULL) {/*if this is the only block in the list*/
                PUT(heap_ptr + list_num, (size_t) NULL); /*the head is NULL now*/
        }
	else if (prev_free_blk == NULL && next_free_blk != NULL) { /*If this the first block pointed to by head of list*/
		PUT(heap_ptr + list_num, (size_t) next_free_blk);/*make the next block the head of the list*/
		PUT(PREV_FREE(next_free_blk), (size_t) NULL);/*set its previosu to NULL*/
	}
	else if (prev_free_blk != NULL && next_free_blk == NULL) {/*if this is the last block in the list*/
                PUT(NEXT_FREE(prev_free_blk), (size_t) NULL);/*make the previous block , the last block*/
        }
	else { /*if the block is in the middle*/
		PUT(NEXT_FREE(prev_free_blk), (size_t) next_free_blk);/*make previous block point to next and next to previous*/
		PUT(PREV_FREE(next_free_blk), (size_t) prev_free_blk);
	}
}

/*
 * Return whether the pointer is in the heap.
 * may be useful for debugging.
 */
static int in_heap(const void *p){
	return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p){
	return (size_t) (((size_t) (p) + 7) & ~0x7) == (size_t) p;
}

/*
 * get_list_size - This function takes in an index and returns back 
 * the size of the list that can be indexed by that index
 */
static size_t get_list_size(size_t index){
	if(index == 0)
		return LIST0_SIZE;
	else if(index == 1)
		return LIST1_SIZE;
	else if(index == 2)
		return LIST2_SIZE;
	else if(index == 3)
                return LIST3_SIZE;
	else if(index == 4)
                return LIST4_SIZE;
	else if(index == 5)
                return LIST5_SIZE;
	else if(index == 6)
                return LIST6_SIZE;
	else if(index == 7)
                return LIST7_SIZE;
	else if(index == 8)
                return LIST8_SIZE;
	else if(index == 9)
                return LIST9_SIZE;
	else
                return LIST10_SIZE;
}

/*
 * check_free_blk_count - This function checks if
 * the number of free blocks counted in heap is same as 
 * the number counted by moving through pointers in seg lists
 */
static int check_free_blk_count(){
	char *bp; /*pointer to the block*/
	char *blk; /*pointer to a block in heap*/
	int cnt_heap = 0; /*number of free blocks by iterating by blocks*/
	int cnt_ptr = 0; /*no of free blocks by iterating through pointers*/
	int i; /*iterate through lists*/
	size_t list_num; /*offset of the list i*/
	
	/*iterate over the free lists by pointers*/
	for(i =0; i < NO_OF_LISTS; i++){ /*traverse through each list*/
		list_num = find_list(i);
		bp = (char *) (heap_ptr + list_num); /*head of the list*/
		bp = (char *)GET(NEXT_FREE(bp));
		while(bp != NULL){/*traverse from one block to another by pointer*/
			cnt_ptr ++;/*count free blocks*/
			bp = (char *)GET(NEXT_FREE(bp));
		}
	}
	
	blk = (NO_OF_LISTS * DSIZE) + (2 * DSIZE) + (heap_ptr); /*pointer to first block in heap*/
	/*Iterate blockwise*/
	while((GET_4(HDRP(blk)) != 1) && (GET_4(HDRP(blk)) != 3)){ /*while epilogues block has not been hit*/
		if(!GET_ALLOC(HDRP(blk))){/*if free, increment count*/
			cnt_heap ++;
		}
		blk = NEXT_BLKP(blk); /*next block*/
	}
	if(cnt_heap != cnt_ptr){/*if counts mismatch*/
		printf("Mismatch in number of free blocks, count by block traversal \
			%d and count by pointer traversal %d", cnt_heap, cnt_ptr);
		return 1;
	}
	return 0;
}

/*
 * check_for_cycle - This function checks if at all
 * there is a cycle in any of the seg lists
 */
static int check_for_cycle(){
	size_t i; /*traverse through each list*/
	size_t list_num; /*offset of the list i*/
	char *hare;/*one iterator of the list*/
	char *tortoise; /*the other iterator of the list*/
 
	for(i = 0; i < NO_OF_LISTS; i++){ /*iterate through each list*/
                list_num = find_list(i);
                hare = (char *) (heap_ptr + list_num);
                tortoise = (char *) (heap_ptr + list_num);
                /*check for a cycle in the list  y using hare and tortoise*/
                while(hare != NULL && tortoise != NULL){
                        if(hare != NULL){
                                hare = (char *) GET(NEXT_FREE(hare));
                        }
                        if(hare != NULL){
                                tortoise = (char *)GET(NEXT_FREE(hare));
                        }
                        if(hare == tortoise){ /*A cycle exists in the list*/
                                printf("Error: There is a cycle in the list\n");
                                return 1;
                        }
                }
        }
	return 0;
}

/*
 * check_seg_lists - This function checks all the segregated lists
 * to see if there is any block that does not fall within the appropriate 
 * range of the segreagted list
 */
static int check_seg_lists(){
	size_t i; /*traverse through the lists*/
	size_t list_num; /*Hold the offset to the list*/
	size_t min_size; /*minimum size range of the list*/
	size_t max_size; /*maximum allowed size in a list*/
	char *list_p; /*pointer to a block in a list*/

	/* Check if all blocks in each freelist fall within the appropriate range*/
	for (i = 0; i < NO_OF_LISTS; i++){
		list_num  =  find_list(i);
		if(i ==0){
			min_size = 0;
                        max_size = LIST0_SIZE;
		}
		 else if(i == 11){
                        min_size = LIST10_SIZE;
                        max_size = ~0;
                }
		else{
                        min_size = get_list_size(i-1);
                        max_size = get_list_size(i);
                }
		list_p = (char *) GET(heap_ptr + list_num);
		while (list_p != NULL){
			if (!(min_size < GET_SIZE(HDRP(list_p)) && GET_SIZE(HDRP(list_p)) <= max_size)){
				printf("The free blk pointer %p is not in the apt free list", list_p);
				return 1;
			}
			list_p = (char *) GET(NEXT_FREE(list_p));
		}
	}
	return 0;
}

/*
 * check_free_blk - This function checks the free block blk
 * for scenarios like : 
 * 1. If the next block has the correct pointer to the current
 * 2. If the prev free block has correct pointers
 * 3. If the header and footer match
 * 4. If the block is aligned
 * 5. If the block lies in the heap
 * 6. If an adjacent block is free, not coalesced
 */
static int  check_free_blk(char *blk){
	if(!in_heap(blk)){ /*if block not in heap*/
		printf("Block not in heap");
		return 1;
	}
        if(!aligned(blk)){ /*if block is not aligned*/
                printf("block not aligned to 8 byte alignment");
		return 1;
	}
	/*check if pointer from next block is inconsistent*/
        if ((char *) GET(NEXT_FREE(blk)) != NULL && (char *) GET(PREV_FREE(GET(NEXT_FREE(blk)))) != blk){
                printf("Free block pointer %p's next pointer is inconsistent\n", blk);
		return 1;
	}
        /*check if pointer from previous block is inconsistent*/
        if ((char *) GET(PREV_FREE(blk)) != NULL && (char *) GET(NEXT_FREE(GET(PREV_FREE(blk)))) != blk){
                printf("Free block pointer %p's previous pointer is inconsistent\n", blk);
		return 1;
	}
        /*check for two adjacent free blocks, if next blk is not epilogue and is free*/
        if ((GET_4(HDRP(NEXT_BLKP(blk))) != 1) && (GET_4(HDRP(NEXT_BLKP(blk))) != 3) && !GET_ALLOC(HDRP(NEXT_BLKP(blk)))){
                printf("Error: Free block pointer %p and %p are adjacent\n", blk, NEXT_BLKP(blk));
		return 1;
	}
        /*check if header and footer are similar*/
        if ((GET_4(HDRP(blk)) != GET_4(FTRP(blk)))){
                printf("Free block pointer %p: header and footer mismatch\n", blk);
		return 1;
	}
	return 0;
}

/*
 * check_alloc_blk - This function checks 
 * an allocated  block for the following:
 * 1. If the block is aligned
 * 2. If the block lies in the heap
 */
static int check_alloc_blk(char *blk){
	if(!in_heap(blk)){ /*if block not in heap*/
                printf("Block not in heap");
		return 1;
	}
        if(!aligned(blk)){ /*if block is not aligned*/
                printf("block not aligned to 8 byte alignment");
		return 1;
	}
	return 0;
}

/*
 * print_free_blk - this function prints all attributes
 * of a free block
 */
static void print_free_blk(char *bp){
	if(bp != NULL){
		dbg_printf("%p : header : [%2d : %c] footer : [%2d : %c]\n", bp, GET_SIZE(HDRP(bp)), \
		(GET_ALLOC(HDRP(bp)) ? 'a' : 'f'), GET_SIZE(FTRP(bp)), \
		(GET_ALLOC(FTRP(bp)) ? 'a' : 'f'));
		dbg_printf("%p : next :[%p] previous : [%p]\n", bp, NEXT_FREE(bp), PREV_FREE(bp));
	}
	else{
		dbg_printf("%p block is null", bp);
	}
}

/*
 * print_alloc_blk - This function prints the attributes of
 * an allocated block
 */
static void print_alloc_blk(char *bp){
	if(bp != NULL){
		dbg_printf("%p : header : [%2d : %c]\n", bp, GET_SIZE(HDRP(bp)), \
		(GET_ALLOC(HDRP(bp)) ? 'a' : 'f'));
	}
	else{
		dbg_printf("%p block is NULL", bp);
	}
}
