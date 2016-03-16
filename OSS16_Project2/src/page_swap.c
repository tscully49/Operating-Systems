#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
// link back store
#include <back_store.h>

#include "../include/page_swap.h"

// MACROS
#define MAX_PAGE_TABLE_ENTRIES_SIZE 2048
#define MAX_PHYSICAL_MEMORY_SIZE 512
#define TIME_INTERVAL 100
#define DATA_BLOCK_SIZE 1024

// helper macro
#define BS_PAGE_MAP(x) ((x) + 8);

/*
 * An individual frame
 * */
typedef struct {
	unsigned int page_table_idx; // used for indexing the page table
	unsigned char data[DATA_BLOCK_SIZE]; // the data that a frame can hold
	unsigned char access_tracking_byte; // used in LRU approx
	unsigned char access_bit; // used in LRU approx
}frame_t;

/*
 * Manages the array of frames
 * */
typedef struct {
	frame_t entries[MAX_PHYSICAL_MEMORY_SIZE]; // creates an frame array
}frame_table_t;

/*
 * An individual page
 * */
typedef struct {
	unsigned int frame_table_idx; // used for indexing the frame table
	unsigned char valid; // used to tell if the page is valid or not
} page_t;


/*
 * Manages the array of pages 
 * */
typedef struct {
	page_t entries[MAX_PAGE_TABLE_ENTRIES_SIZE]; // creates an page array
}page_table_t;


/*
 * CONTAINS ALL structures in one structure
 * */
typedef struct {

frame_table_t frame_table;
page_table_t page_table;
back_store_t* bs;

}page_swap_t;


// A Global variable that is used in the following page
// swap algorithms
static page_swap_t ps; 


// function to populate and fill your frame table and page tables
// do not remove
bool initialize (void) {

	// needs work to create back_store properly
	back_store_t* bs = back_store_create("PAGE_SWAP");
	ps.bs = bs;
	
	unsigned char buffer[1024] = {0};
	// requests the blocks needed
	for (int i = 0; i < MAX_PAGE_TABLE_ENTRIES_SIZE; ++i) {
		if(!back_store_request(ps.bs,i+8)) {
			fputs("FAILED TO REQUEST BLOCK",stderr);
			return false;
		}
		// create dummy data for blocks
		for (int j = 0; j < 1024; ++j) {
			buffer[j] = j % 255;
		}
		// fill the back store
		if (!write_to_back_store (buffer,i)) {
			fputs("FAILED TO WRITE TO BACK STORE",stderr);
			return false;
		}
	}

	/*zero out my tables*/
	memset(&ps.frame_table,0,sizeof(frame_table_t));
	memset(&ps.page_table,0,sizeof(page_table_t));

	/* Fill the Page Table and Frame Table from 0 to 512*/
	frame_t* frame = &ps.frame_table.entries[0];
	page_t* page = &ps.page_table.entries[0];
	for (int i = 0;i < MAX_PHYSICAL_MEMORY_SIZE; ++i, ++frame, ++page) {
		// update frame table with page table index
		frame->page_table_idx = i;
		// set the most significant bit on accessBit
		frame->access_bit = 128;
		// assign tracking byte to max time
		frame->access_tracking_byte = 255;
		/*
		 * Load data from back store
		 * */
		unsigned char* data = &frame->data[0];
		if (!read_from_back_store (data,i)) {
			fputs("FAILED TO READ FROM BACK STORE",stderr);
			return false;
		}
		// update page table with frame table index
		page->frame_table_idx = i;
		page->valid = 1;
		
	}
	return true;
}
// keep this do not delete
void destroy(void) {
	back_store_close(ps.bs);
}

/*
 * ALRU IMPLEMENTATION : TODO IMPLEMENT
 * */

page_request_result_t* approx_least_recently_used (const uint16_t page_number, const size_t clock_time) {		
	if (page_number >= MAX_PAGE_TABLE_ENTRIES_SIZE) return NULL;

	int victim = 0; // Set the first min page number to page 0
	page_request_result_t* page_req_result = NULL; // define the return variable 
	page_t* page = &ps.page_table.entries[page_number]; // pointer to the entry in the page table
	frame_t* frame = NULL; // two more pointers 
	frame_t* min_frame = NULL;
	if (page->valid) { // check if the frame in the page table is valid
		victim = page->frame_table_idx;
		page_req_result = NULL; // Return NULL if it is
	} else {
		frame = &ps.frame_table.entries[0]; // define the frame pointer to the first entry in the frame table
		min_frame = &ps.frame_table.entries[victim]; // set the minimum frame to the smallest page number in the frame table
		for (int j=0; j < MAX_PHYSICAL_MEMORY_SIZE; ++j, ++frame) { // find the victim for the page swap 
			if (frame->access_tracking_byte <= min_frame->access_tracking_byte) { // if page frame access byte is smaller than victim
				victim = j; // change the min page number
				min_frame = &ps.frame_table.entries[victim]; // set the min frame pointer to the new min frame
			}
		}
		// Create page_request_result_t struct and put values in it before swapping
		page_req_result = (page_request_result_t *)malloc(sizeof(page_request_result_t));
		if (page_req_result == NULL) {
			printf("\nProblem Mallocing memory for return variable");
			return NULL;
		}
		page_req_result->page_requested = page_number;
		page_req_result->page_replaced = ps.frame_table.entries[victim].page_table_idx;
		page_req_result->frame_replaced = victim;

		// Write frame to BS, then read Page from BS and add to Frame
		if (!write_to_back_store(&(ps.frame_table.entries[victim].data), ps.frame_table.entries[victim].page_table_idx)) printf("\nWRITE to BACK STORE failed");
		if (!read_from_back_store(&(ps.frame_table.entries[victim].data), page_number)) printf("\nRead from BACK STORE failed");
	}

	ps.frame_table.entries[victim].access_bit = 128; // set access bit to 1 

	frame = &ps.frame_table.entries[0]; // start frames from beginning
	if (clock_time % TIME_INTERVAL == 0) { // once the time iterval hits
		for (int i=0; i<MAX_PHYSICAL_MEMORY_SIZE; ++i, ++frame) {
			frame->access_tracking_byte = frame->access_tracking_byte >> 1; // shift right one
			frame->access_tracking_byte |= frame->access_bit; // set the MSB 
		}
	}

	return page_req_result;
}


/*
 * LFU IMPLEMENTATION : TODO IMPLEMENT
 * */
page_request_result_t* least_frequently_used (const uint16_t page_number, const size_t clock_time) {
	if (page_number >= MAX_PAGE_TABLE_ENTRIES_SIZE) return NULL;

	int victim = 0; // Set the first min page number to page 0
	page_request_result_t* page_req_result = NULL; // define the return variable
	page_t* page = &ps.page_table.entries[page_number]; // pointer to the entry in the page table
	frame_t* frame = NULL; // two more pointers 
	frame_t* min_frame = NULL;
	if (page->valid) { // check if the frame in the page table is valid
		victim = page->frame_table_idx;
		page_req_result = NULL; // Return NULL if it is
	} else {
		frame = &ps.frame_table.entries[0]; // define the frame pointer to the first entry in the frame table
		min_frame = &ps.frame_table.entries[victim]; // set the minimum frame to the smallest page number in the frame table
		for (int j=0; j < MAX_PHYSICAL_MEMORY_SIZE; ++j, ++frame) { // find the victim for the page swap 
			if (countSetBits(frame->access_tracking_byte) <= countSetBits(min_frame->access_tracking_byte)) { // if page frame NUMBER OF SET BITS is smaller than victim
				victim = j; // change the min page number
				min_frame = &ps.frame_table.entries[victim]; // set the min frame pointer to the new min frame
			}
		}
		// Create page_request_result_t struct and put values in it before swapping
		page_req_result = (page_request_result_t *)malloc(sizeof(page_request_result_t));
		if (page_req_result == NULL) {
			printf("\nProblem Mallocing memory for return variable");
			return NULL;
		}
		page_req_result->page_requested = page_number;
		page_req_result->page_replaced = ps.frame_table.entries[victim].page_table_idx;
		page_req_result->frame_replaced = victim;

		// Write frame to BS, then read Page from BS and add to Frame
		if (!write_to_back_store(&(ps.frame_table.entries[victim].data), ps.frame_table.entries[victim].page_table_idx)) printf("\nWRITE to BACK STORE failed");
		if (!read_from_back_store(&(ps.frame_table.entries[victim].data), page_number)) printf("\nRead from BACK STORE failed");
	}

	ps.frame_table.entries[victim].access_bit = 128; // set access bit to 1 

	frame = &ps.frame_table.entries[0]; // start frames from beginning
	if (clock_time % TIME_INTERVAL == 0) { // when the time interval hits 
		for (int i=0; i<MAX_PHYSICAL_MEMORY_SIZE; ++i, ++frame) {
			frame->access_tracking_byte = frame->access_tracking_byte >> 1; // shift right one
			frame->access_tracking_byte |= frame->access_bit; // set the MSB 
		}
	}

	//page_request_result_t* page_req_result = NULL;
	return page_req_result;
}


/*
 * BACK STORE WRAPPER FUNCTIONS: TODO IMPLEMENT
 * */
bool read_from_back_store (void *data, const unsigned int page) {
	if (!data || page >= MAX_PAGE_TABLE_ENTRIES_SIZE) return false; // check parameters

	unsigned int map = BS_PAGE_MAP(page); // make sure we are not accessing the FBM
	
	return back_store_read(ps.bs, map, data); // read data from BackStore
}

bool write_to_back_store (const void *data, const unsigned int page) {
	if (!data || page >= MAX_PAGE_TABLE_ENTRIES_SIZE) return false; // check parameters

	unsigned int map = BS_PAGE_MAP(page); // make sure we are not accessing the FBM
	
	return back_store_write(ps.bs, map, data); // write data to the backing store
}

/* Function gets number of set bits in binary
   representation of passed binary number. */
int countSetBits(unsigned char ch) {
    unsigned int c; 
    for( c = 0; ch; ch >>=1)
        c += ch & 1;
     return c;
} // Helper function used to count bits in a number