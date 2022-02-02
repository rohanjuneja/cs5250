//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*

  This file does NOT implement any prefetcher, and is just an outline

 */

#include <stdio.h>
#include "../inc/prefetcher.h"
#include <queue>
#define ull unsigned long long int
#define ul unsigned long int
#define INDEX_TABLE_SIZE 256
#define GHB_SIZE 256
#define LOOKUP_DEPTH 16
#define PREFETCH_DEGREE 4
#define MATCH_DEGREE 4
using namespace std;

typedef struct ghb_entry {
	ull ip;
	ull addr;
	ghb_entry *link;	
} ghb_entry;

typedef struct {
	ull ip;
	ull addr;
	int index;
} it_entry;

ull head_ptr;
deque<it_entry> index_table;
ghb_entry ghb[GHB_SIZE];
ull deltas[GHB_SIZE];
int current_index = -1;


it_entry* find_it(ull ip) {
	// printf("%u\n", index_table.size()); 
	for(unsigned int iter=0; iter<index_table.size(); iter++) {
		if(index_table[iter].ip == ip)
			return &index_table[iter];
	}
	// printf("no index found in index_table\n");
	return NULL;
}

void print_delta(long long int delta[], int size) {
	printf("Delta\n");
	for(int i=0; i<size; i++) {
		printf("%d %llx\t", i, delta[i]);
	}
	printf("\n");
}

void print_prefetch(ull prefetch[], int size) {
	printf("Prefetch\n");
	for(int i=0; i<size; i++) {
		printf("%d %llx\t", i, prefetch[i]);
	}
	printf("\n");
}

void delta_correlation(ull ip, ull addr) {
	
	// int top = -1;
	long long int delta[LOOKUP_DEPTH] = {0};
	ull prefetch[PREFETCH_DEGREE] = {0};

	ghb_entry *cur_ip, *prev_ip; 
	cur_ip = &ghb[find_it(ip)->index];

	if(cur_ip->link == NULL) {
		return;
	}
	// if(index_it == 1) {
	//	printf("Not in index_table\n");
	//  	return;
	// }
	// cur_ip = index_table[find_it(ip)].link;
	
	for(int i=LOOKUP_DEPTH-1; i>=0; i--) {
		prev_ip = cur_ip->link;

	
		if((prev_ip == NULL) || (prev_ip->addr == cur_ip->addr) || (prev_ip->ip != cur_ip->ip)) {
			// printf("Delta broke\n");
			break;
		}

		delta[i] = cur_ip->addr - prev_ip->addr;
	
		cur_ip = prev_ip;
	}
	
	// print_delta(delta, LOOKUP_DEPTH);
	
	int found_match = -1;
	for(int i=0; i<LOOKUP_DEPTH-MATCH_DEGREE; i++) {
		
		if(delta[i] == delta[LOOKUP_DEPTH-2] && delta[i+1] == delta[LOOKUP_DEPTH-1]) {
			found_match = i+2;
			break;
		}
	
	}

	if(found_match == -1) {
		// printf("No match found\n");
		return;
	}

	// printf("Found match %d\n", found_match);

	prefetch[0] = addr + delta[found_match];
	
	for(int i=1; i<PREFETCH_DEGREE; i++) {
		if( (found_match+i < LOOKUP_DEPTH) && (delta[i]!=0) ) {
			prefetch[i] = prefetch[i-1] + delta[i];
			if(get_l2_mshr_occupancy(0) < PREFETCH_DEGREE)
	      			l2_prefetch_line(0, addr, prefetch[i], FILL_L2);
		}
	}

	// print_prefetch(prefetch, PREFETCH_DEGREE);

}

void print_it() {
	printf("Index table\n");
	
	for(unsigned int i=0; i<index_table.size(); i++)
		printf("%u %llx %llx %d\n", i, index_table[i].ip, index_table[i].addr, index_table[i].index);
	
	printf("\n");
}

void print_ghb() {
	printf("GHB Table\n");
	ghb_entry *temp;

	for(unsigned int i=0; i<GHB_SIZE; i++)
		if(ghb[i].link == NULL)
			printf("%u %llx %llx\t", i, ghb[i].ip, ghb[i].addr);
		else {
			printf("%u %llx %llx ", i, ghb[i].ip, ghb[i].addr);
			temp = &ghb[i];
			while(temp->link != NULL) {
				printf("%llx ", temp->link->addr);
				temp = temp->link;
			}
			printf("\t");
		}
	printf("\n\n");
}

void l2_prefetcher_initialize(int cpu_num)
{
  printf("Initialize Prefetching\n");
  
  head_ptr = -1;
  current_index = -1;

 //  for(int i=0; i<INDEX_TABLE_SIZE; i++) {
 //  	index_table[i].addr = 0;
 //        index_table[i].link = NULL;
 //  }

  for(int i=0; i<GHB_SIZE; i++) {
	ghb[i].ip = 0;
	ghb[i].addr = 0;
        ghb[i].link = NULL;
  }

  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{

  // printf("(0x%llx 0x%llx %d %d %d) \n", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));
  ghb_entry *ghb_obj;
  it_entry *it_obj, *it1_obj;
  ghb_obj = new ghb_entry;
  it1_obj = new it_entry;
  ghb_obj->ip = ip;
  ghb_obj->addr = addr;
  ghb_obj->link = NULL;

  it_obj = find_it(ip);
  if(it_obj == NULL) {
  	// printf("First condition\n");
	current_index = (current_index + 1) % GHB_SIZE;
	ghb[current_index] = *ghb_obj;
  	
	it1_obj->ip = ip;
	it1_obj->addr = addr;
	it1_obj->index = current_index;

	index_table.push_back(*it1_obj);
	if(index_table.size() >= INDEX_TABLE_SIZE) {
	 	index_table.pop_front();
	}

	delta_correlation(ip, addr);

  }
  else {
  	// printf("Second condition\n");
	
	ghb_obj->link = &ghb[it_obj->index];
	current_index = (current_index + 1) % GHB_SIZE;
	ghb[current_index] = *ghb_obj;
  	
	it_obj->addr = addr;
	it_obj->index = current_index;
	
	delta_correlation(ip, addr);
  }
  
  // print_it();
  // print_ghb();

  delete ghb_obj;
  delete it1_obj;
  
  // uncomment this line to see all the information available to make prefetch decisions
  // printf("(0x%llx 0x%llx %d %d %d) \n", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));
}

void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
  // uncomment this line to see the information available to you when there is a cache fill event
  //printf("0x%llx %d %d %d 0x%llx\n", addr, set, way, prefetch, evicted_addr);
}

void l2_prefetcher_heartbeat_stats(int cpu_num)
{
  printf("Prefetcher heartbeat stats\n");
}

void l2_prefetcher_warmup_stats(int cpu_num)
{
  printf("Prefetcher warmup complete stats\n\n");
}

void l2_prefetcher_final_stats(int cpu_num)
{
  printf("Prefetcher final stats\n");
}
