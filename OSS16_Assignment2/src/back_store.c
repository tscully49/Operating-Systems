#include "../include/back_store.h"

#define NUM_BLOCKS 65536
#define NUM_FBM_BLOCKS 8
#define BLOCK_SIZE 1024
#define FBM_SIZE 8192

struct back_store {
	bitmap_t* FBM;
	int fd;
};

// Creates a backing store file, initializing all blocks to 0 to start off 
back_store_t *back_store_create(const char *const fname) {
	if (!fname) return NULL; // Error check parameters 

	if(strcmp(fname, "\n")==0 || strcmp(fname, "\0") == 0 || strcmp(fname, "") == 0) return NULL; // error check file names
	
	// Create the file, or truncate it if it already exists
	int fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // open file, create it, truncate it, and open for write only
	if (fd == -1) { // check that file opened and created successfully
		printf("\nOpening file : Failed\n"); // Print out error if opening failed 
        printf ("Error no is : %d\n", errno);
        printf("Error description is : %s\n",strerror(errno));
		return NULL;
	}

	back_store_t* output = (back_store_t *)malloc(sizeof(back_store_t)); //malloc memory for back_store struct
	if (output == NULL) return NULL; // error check malloc
	
	bitmap_t* new_bitmap = bitmap_create(FBM_SIZE * 8); // Create a bitmap for the number of bits in the bitmap
	if (new_bitmap != NULL) { // Check that the bitmap creation worked
		output->FBM = new_bitmap; // add bitmap to the struct 
		output->fd = fd; // add file descriptor to the struct
	} else {
		return NULL;
	}

	for (int i=0 ; i<NUM_FBM_BLOCKS ; ++i) { // Set the bits for the FBM innnn the FBM to 1's
		bitmap_set(new_bitmap, i);
	}

	uint8_t buffer[BLOCK_SIZE]; // create a buffer
	memset(buffer, 0x00, BLOCK_SIZE); // set buffer to 0
	for (off_t i=0 ; i<NUM_BLOCKS ; ++i ) { // create entire bs file all initialized to 0 
		write(fd, buffer, BLOCK_SIZE); // write to the bs file
	}

	return output; // return object if no failures
}

back_store_t *back_store_open(const char *const fname) {
	if (!fname) return NULL; // Error check parameters 

	if(strcmp(fname, "\n")==0 || strcmp(fname, "\0") == 0 || strcmp(fname, "") == 0) return NULL; // error check file names
	// Create the file, or truncate it if it already exists
	int fd = open(fname, O_RDWR); // open file, create it, truncate it, and open for write only
	if (fd == -1) { // check that opening the file worked correctly
		printf("\nOpening file : Failed\n"); // output error if opening file did not work
        printf ("Error no is : %d\n", errno);
        printf("Error description is : %s\n",strerror(errno));
		return NULL;
	}

	back_store_t* output = (back_store_t *)malloc(sizeof(back_store_t)); // malloc memory for back_store struct
	if (output == NULL) return NULL; // Error check the malloc 
	uint8_t bitmap_buffer[FBM_SIZE]; // Create buffer for the bitmap data from the bs file

	// READ the bitmap from file (first 8KB) to the FBM attribute
	if (lseek(fd, 0, SEEK_SET) >= 0) { // Go to beginning of file 
		if (read(fd, bitmap_buffer, FBM_SIZE) > 0) { // Read the bitmap DATA from the bs file 
			bitmap_t* new_bitmap = bitmap_import(FBM_SIZE * 8, bitmap_buffer); // create a bitmap with the read data
			if (new_bitmap != NULL) { // check that the import worked correctly 
				output->FBM = new_bitmap; // add bitmap to struct
				output->fd = fd; // add the file descriptor to the struct
			} 
		}
	} else {
		return NULL;
	}

	return output; // return object if no failures
}

void back_store_close(back_store_t *const bs) {
	if (!bs) return; // Error check parameters 

	const uint8_t *bitmap_buffer = bitmap_export(bs->FBM); // Export the bitmap data to a buffer
	if (bitmap_buffer != NULL) { // check that the export worked correctly 
		if (lseek(bs->fd, 0, SEEK_SET) >= 0) { // seek to beginning of bs file 
			if (write(bs->fd, bitmap_buffer, FBM_SIZE) == -1) { // write to the bs file 
				printf("\nERROR writing bitmap data to file.");
			}
		}
	} else {
		printf("\nERROR Writing bitmap to data.");
	}

	if (close(bs->fd) == -1) { // check that the file closes correctly
		printf("\nERROR CLOSING BS FILE."); 
		bitmap_destroy(bs->FBM); // destroy bitmap
		free(bs); // free memory
		return;
	}

	bitmap_destroy(bs->FBM); // destroy bitmap
	free(bs); // free memory

	return;
}

unsigned back_store_allocate(back_store_t *const bs) {
	if (!bs) return 0; // error check parameters

	//check bitmap for an unset bit, set the bit, and return the "block number" that the bit represents
	size_t allocated_bit = bitmap_ffz(bs->FBM);
	if (allocated_bit == SIZE_MAX) { // error check the bitmap_ffz function
		return 0; 
	} 
	bitmap_set(bs->FBM, allocated_bit); // set the bit in the FBM for the block that has been allocated

	return allocated_bit; // return the bit number from the FBM for the block in the bs file that was allocated
}

bool back_store_request(back_store_t *const bs, const unsigned block_id) {
	if (!bs || !block_id || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false; // error check params

	if (bitmap_test(bs->FBM, block_id) == true) { // check if the requested block is already allocated
		return false;
	} else { // otherwise, allocate the block and set the correct bit in the FBM
		bitmap_set(bs->FBM, block_id);
		return true;
	}
}

void back_store_release(back_store_t *const bs, const unsigned block_id) {
	if (!bs || !block_id || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return; // error check params
	bitmap_reset(bs->FBM, block_id); // set the bit in the FBM to 0
	return;
}

bool back_store_read(back_store_t *const bs, const unsigned block_id, void *const dst) {
	if (!bs || !block_id || !dst || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false; // error check params

	if ((lseek(bs->fd, (block_id * BLOCK_SIZE), SEEK_SET) >= 0)) { // seek to the block of the bs file that we need
		if (read(bs->fd, dst, BLOCK_SIZE) > -1 ) { // read that block from the bs file to the dst 
			return true; 
		} else { // print out errors with reading from the bs file
		    printf("\nValue of errno: %d\n", errno);
		    printf("Error opening file: %s\n\n", strerror( errno ));
		}
	}
	return false; // return false if the read did not work correctly 
}

bool back_store_write(back_store_t *const bs, const unsigned block_id, const void *const src) {
	if (!bs || !block_id || !src || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false; // error check params
	if (lseek(bs->fd, (off_t)(block_id * BLOCK_SIZE), SEEK_SET) >= 0) { // seek to the block in the bs file that needs to be written
		if (write(bs->fd, src, BLOCK_SIZE) > -1 ) { // write to the block in the bs file
			return true;
		}
	}
	return false; // return false if the read did not work correctly 
}
