#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <malloc.h>
#include <queue>
using namespace std;

#define MAX_PAGES 250
#define MAX_FRAMES 512
#define MAX_ORDER 9//(int)log2(MAX_FRAMES)
#define NR_ACTIVE MAX_PAGES

static int log_ceil(int n) {
	int k=1, l=0; 
	while(n>k) {
		l++; k*=2;
	}  
	return l;
}

typedef struct llist{
	int free, index, size;
	int inactive, active;
	struct llist *next;
} llist;


typedef struct lru{
	int index;
	struct lru *next;
} lru;

llist frames[MAX_FRAMES];
llist *buckets[MAX_ORDER+1];
deque<int> active, inactive;

// To keep a count of total pages allocated at any given time
int global_counter = 0;


// Printing lru for debug purposes
void print_lru() {
	printf("Print lru\n");
	
	printf("Printing inactive list with size %d\n", inactive.size());
	for(unsigned int i=0; i<inactive.size(); i++)
		printf("%d %d %d\t", inactive[i], frames[inactive[i]].inactive, frames[inactive[i]].active);
	printf("\n");

	printf("Printing active list with size %d\n", active.size());
	for(unsigned int i=0; i<active.size(); i++)
		printf("%d %d %d\t", active[i], frames[active[i]].inactive, frames[active[i]].active);
	printf("\n\n");
}
	
// Printing buckets for debug purposes and final output
void print_buckets() {
	printf("Printing buckets\n");
	printf("The first part of any element represents the index from where the set of frames start, and the second part of the element represents order n (for which size will be 2^n)\n"); 
	for(int i=0; i<=MAX_ORDER; i++) {
		printf("bucket[%d] ", i);

		llist *temp = buckets[i];
		while(temp != NULL) {
			printf("%d %d %d\t", temp->index, temp->free, temp->size);
			temp = temp->next;
		}
		printf("\n");
	}
	
	printf("\n");
}

// Initialising all the frames
void init_list() {
	for(int i=0; i<MAX_FRAMES; i++) {
		frames[i].index = i;
		frames[i].free = 1;
		frames[i].next = NULL;
		frames[i].size = 1;
		frames[i].inactive = false;
		frames[i].active = false;
	}
	// Setting the first frame with MAX_ORDER to represent a set of all pages
	frames[0].size = MAX_ORDER;
}

// Initialising buckets
void init_buckets() {
	for(int i=0; i<MAX_ORDER; i++)
		buckets[i] = NULL;
	
	buckets[MAX_ORDER] = &frames[0];
}

// Push a page to the bucket of it's order
void push_to_list(llist* entry) {
	printf("Push to list %d\n", entry->index);
	llist **list = &buckets[entry->size];
	
	// This will inserted in a sorted way, so that the frames with higher index can be reclaimed easily
	while(*list != NULL) {
		if((*list)->index > entry->index)
			break;
		list = &((*list)->next);
	}
		entry->next = *list;
		*list = entry;
}

// Removing a page from a bucket
void remove_from_list(llist* entry, llist** list) {
	
	llist **temp = list;
	while((*temp) != NULL)  {
		if((*temp) == entry) {
			(*temp) = entry->next;
			break;
		}
		temp = &((*temp)->next);
	}
}

// Spliting a block of order n into two blocks of order n-1
llist* split(llist* block, int order) {

	if(order == 0)
		return block;

	order -= 1;
	buckets[order] = block + (1 << order);

	printf("Split %d\n", (block + (1<< order))->index);	
	block->size = (block->size)-1;
	(block + (1<<order))->size = block->size;
	
	return block;
}

// Allocate frames to a set of pages
llist* allocate(int num) {

	// If more then 2^9 pages want to be allocated, it will return. And the code in the main function will throw an error.
	if(num > MAX_FRAMES) return NULL;
	
	// Find the ceil of log of the number of pages
	int order = log_ceil(num);

	// Return frames if buckets with that particular order has any frames.
	if( buckets[order] ) {
		llist* block = buckets[order];
		remove_from_list(block, &buckets[order]);
		block->free = 0;
		// print_buckets();
		return block;
	}

	// If not, it will look for frames in buckets of higher order
	llist* block;
	int found_block_order;
	for(found_block_order=order+1; found_block_order<=MAX_ORDER; found_block_order++) {
		block = buckets[found_block_order];
		if(block) {
			buckets[found_block_order] = block->next;
			break;
		}
	}

	// print_buckets();
	// If the block is not found, it will return. Then the main function will divide the page requests of n into two requests of n/2 and n/2
	if(!block) {
		 return NULL;
	}

	// If it finds set of frames with higher order of m, it will split it into two of order m/2 and m/2. It will push one of the set of blocks into the buckets of order m/2 and return the other one
	while(order < found_block_order) {
		block = split(block, found_block_order);
		found_block_order -= 1;
		// print_buckets();
	}

	block->free = 0;
	
	// Once the block has been split into the specific order it will return	
	return block;
}
 
// This function will always be called from inside free
// It will reclaim and merge the frames with their buddy 
void reclaim(llist *block, int order) {
	while(1) {
		int temp = (block->index)/(1<<order);
	
		llist *buddy;
	
		// Calculate the buddy
		if(temp % 2 == 0)
			buddy = block + (1 << order);
		else
			buddy = block - (1 << order);
	
		// If buddy is not free or the size if different from the frame, it will return
		if(!buddy->free || (buddy->size != block->size))
			return;
		
		// Remove from buckets of order n
		remove_from_list(block, &buckets[order]);
		remove_from_list(buddy, &buckets[order]);
		block->free = 1;
		
		// Increase the order to n+1
		order += 1;
		if(temp%2 != 0)
			block = buddy;
		block->size++;
		
		push_to_list(block);
		// print_buckets();
	}
}

// Free one page
void free_mem(int page_no) {
	
	printf("Free %d\n", page_no);
	// If the page is already freed, return
	if(frames[page_no].free) return;

	// If the page is in inactive list, erase it from the list
	for(auto i=inactive.begin(); (i-inactive.begin()) < inactive.size(); i++) {
		if(inactive[i-inactive.begin()] == page_no) {
			inactive.erase(i);
			break;
		}
	}

	// Similary if the page is in the active list, erase from it
	for(auto i=active.begin(); (i-active.begin()) < active.size(); i++) {
		if(active[i-active.begin()] == page_no) {
			active.erase(i);
			break;
		}
	}

	frames[page_no].inactive = 0;
	frames[page_no].active = 0;
	
	
	global_counter -= 1;
	
	// Set the structure parameter of free to 1	
	frames[page_no].free = 1;
	
	// Push to buckets of order 1 in this case. As only a single page is freed in this assignment
	push_to_list(&frames[page_no]);
	
	// After freeing, reclaim and merge it with it's buddies
	reclaim(&frames[page_no], frames[page_no].size);
	// print_buckets();
	// print_lru();
}

// Access the page and add to the respective list
void access(int page_no) {
	printf("Access %d S %d F %d I %d A %d\n", page_no, frames[page_no].size, frames[page_no].free, frames[page_no].inactive, frames[page_no].active);	
	if(frames[page_no].free) {
		printf("Reached 1\n");
		return;
	}

	// If frame is not in the active or inactive list, it will be added to the inactive list
	if(!frames[page_no].inactive && !frames[page_no].active) {
		printf("Reached 2 %d\n", inactive.size());
		frames[page_no].inactive = 1;
		inactive.push_front(page_no);
		// If inactive list is overflowing, free the last page in FIFO fashion
		if(inactive.size() > MAX_PAGES) {
			printf("Reached 2.1\n");
			int free_page = inactive[inactive.size()-1];
			inactive.pop_back();
			frames[free_page].inactive = 0;
			free_mem(free_page);
		}
	}
	// If the page is already in the inactive list, but not in the active list. It will be added to the active list
	else if(!frames[page_no].active) {
		printf("Reached 3\n");
		frames[page_no].inactive = 0;
		frames[page_no].active = 1;
		for(auto i=inactive.begin(); (i-inactive.begin()) < inactive.size(); i++) {
			if(inactive[i-inactive.begin()] == page_no) {
				inactive.erase(i);
				break;
			}
		}
		
		active.push_front(page_no);
		frames[page_no].active = 1;
		// Similar to inactive list, if active list is overflowing, add the last page to inactive list.
		if(active.size() > MAX_PAGES) {
			int free_page = active[active.size()-1];
			printf("Reached 3.1 %d\n", free_page);
			active.pop_back();
			frames[free_page].active = 0;
			inactive.push_front(free_page);
			frames[free_page].inactive = 1;
		}
	}		
	
	// print_lru();
}

int main() {
	int *seq_arr[1218];
	init_list();
	init_buckets();
	char oper;
	int seq, num, num1;
	llist *temp;
	while( scanf("%c %d %d\n", &oper, &seq, &num) != EOF) {
		printf("Command %c %d %d\n", oper, seq, num);
		int num_orig = num;
		if(oper == 'A') {
			seq_arr[seq] = (int *)malloc(sizeof(int) * num);
			num1 = pow(2, (int)log2(num));

			int count = 0;
			while(num != 0) {
				temp = allocate(num1);
				
				while(num1!=1 && temp == NULL) {
					num1 = num1/2;
					temp = allocate(num1);
				}
				if(temp == NULL)
					break;

				for(int i=0; i<num1; i++) {
					(temp+i)->size = 0;
					(temp+i)->free = 0;
					(temp+i)->inactive = 0;
					(temp+i)->active = 0;
					seq_arr[seq][count++] = (temp+i)->index;
					access((temp+i)->index);
				}
				global_counter += num1;
				num -= num1;
				num1 = pow(2, (int)log2(num));
			}
			// Printing what pages are allocated to the sequence of inputs
			/* for(int i=0; i<num_orig; i++) {
			int temp_seqnum = seq_arr[seq][i];
				printf("%d %d %d %d\t", temp_seqnum, frames[temp_seqnum].index, frames[temp_seqnum].free, frames[temp_seqnum].size);
			}
			printf("\n"); */
			
		
			// Throws error if no pages are allocated to a request. This case doesn't arrive in the current input
			if(temp == NULL) {
				printf("ERROR: No blocks found\n");
				printf("\n\n");
			}
		}
	
		else if(oper == 'X') {
			
			access(seq_arr[seq][num]);
		}
		else if(oper == 'F') {
			free_mem(seq_arr[seq][num]);
		}

		if((inactive.size() + active.size() ) != global_counter)
			printf("ERROR: Size is not maching\n");
		
		print_buckets();
		print_lru();
		
		printf("Total allocated pages %d\n", global_counter);
	}
	
	// Printing the final output	
	print_buckets();
	
	print_lru();
		
	// Printing total number of pages allocated in the end
	printf("Total allocated pages %d\n", global_counter);
	return 0;
}
