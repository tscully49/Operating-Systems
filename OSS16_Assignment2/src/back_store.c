#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <dyn_array.h>
#include <errno.h>
#include <unistd.h>
#include "../include/back_store.h"

#define NUM_BLOCKS 65536
#define NUM_FBM_BLOCKS 8
#define BLOCK_SIZE 1024
#define FBM_SIZE 8192

struct back_store {
	bitmap_t* FBM;
	int fd;
};

back_store_t *back_store_create(const char *const fname) {
	if (!fname) return NULL;

	if(strcmp(fname, "\n")==0 || strcmp(fname, "\0") == 0 || strcmp(fname, "") == 0) return NULL; // error check file names
	// Create the file, or truncate it if it already exists
	int fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // open file, create it, truncate it, and open for write only
	if (fd == -1) {
		printf("\nOpening file : Failed\n");
        printf ("Error no is : %d\n", errno);
        printf("Error description is : %s\n",strerror(errno));
		return NULL;
	}

	// malloc all memory for 
	back_store_t* output = (back_store_t *)malloc(sizeof(back_store_t));
	if (output == NULL) return NULL;
	bitmap_t* new_bitmap = bitmap_create(BLOCK_SIZE * NUM_FBM_BLOCKS * 8 /* 8KB in bits */);
	if (new_bitmap != NULL) {
		output->FBM = new_bitmap; // create bitmap with 8*2^10 bytes (8KB) or 8*8*2^10 bits 
		output->fd = fd;
	} else {
		return NULL;
	}

	for (int i=0 ; i<NUM_FBM_BLOCKS ; ++i) {
		bitmap_set(new_bitmap, i);
	}

	uint8_t buffer[BLOCK_SIZE];
	memset(buffer, 0x00, BLOCK_SIZE);
	for (off_t i=0 ; i<NUM_BLOCKS ; ++i ) {
		write(fd, buffer, BLOCK_SIZE);
	}

	// return object if no failures
	return output;
}

back_store_t *back_store_open(const char *const fname) {
	if (!fname) return NULL;
	// malloc all memory for 
	if(strcmp(fname, "\n")==0 || strcmp(fname, "\0") == 0 || strcmp(fname, "") == 0) return NULL; // error check file names
	// Create the file, or truncate it if it already exists
	int fd = open(fname, O_RDWR); // open file, create it, truncate it, and open for write only
	if (fd == -1) {
		printf("\nOpening file : Failed\n");
        printf ("Error no is : %d\n", errno);
        printf("Error description is : %s\n",strerror(errno));
		return NULL;
	}

	back_store_t* output = (back_store_t *)malloc(sizeof(back_store_t));
	if (output == NULL) return NULL;
	uint8_t bitmap_buffer[BLOCK_SIZE * NUM_FBM_BLOCKS];

	// READ the bitmap from file (first 8KB) to the FBM attribute
	if (lseek(fd, 0, SEEK_SET) >= 0) {
		if (read(fd, bitmap_buffer, BLOCK_SIZE * NUM_FBM_BLOCKS) > 0) {
			bitmap_t* new_bitmap = bitmap_import(BLOCK_SIZE * NUM_FBM_BLOCKS * 8, bitmap_buffer);
			if (new_bitmap != NULL) {
				//printf("\n\n\nBITS: %zu\n\n\n", bitmap_get_bits(new_bitmap));
				output->FBM = new_bitmap; // FILL BITMAP WITH THE IMPORT FROM FILE
				output->fd = fd;
			} 
		}
	} else {
		return NULL;
	}

	// return object if no failures
	return output;
}

void back_store_close(back_store_t *const bs) {
	if (!bs) return;

	const uint8_t *bitmap_buffer = bitmap_export(bs->FBM);
	if (bitmap_buffer != NULL) {
		if (lseek(bs->fd, 0, SEEK_SET) >= 0) {
			write(bs->fd, (void *)bitmap_buffer, BLOCK_SIZE * NUM_FBM_BLOCKS);
			// ERROR CHECK WRITE 
		}
	} else {
		printf("\nERROR Writing bitmap to file");
	}

	if (close(bs->fd) == -1) {
		printf("\n\nERROR CLOSING BS FILE\n");
		bitmap_destroy(bs->FBM);
		free(bs);
		return;
	}

	bitmap_destroy(bs->FBM);
	free(bs);

	return;
}

unsigned back_store_allocate(back_store_t *const bs) {
	if (!bs) return 0;

	//check bitmap for an unset bit, set the bit, and return the "block number" that the bit represents
	size_t allocated_bit = bitmap_ffz(bs->FBM);
	if (allocated_bit == SIZE_MAX) {
		return 0;
	} 
	bitmap_set(bs->FBM, allocated_bit);

	return allocated_bit;
}

bool back_store_request(back_store_t *const bs, const unsigned block_id) {
	if (!bs || !block_id || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false;

	if (bitmap_test(bs->FBM, block_id) == true) {
		return false;
	} else {
		bitmap_set(bs->FBM, block_id);
		return true;
	}
}

void back_store_release(back_store_t *const bs, const unsigned block_id) {
	if (!bs || !block_id || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return;
	bitmap_reset(bs->FBM, block_id);
	return;
}

bool back_store_read(back_store_t *const bs, const unsigned block_id, void *const dst) {
	if (!bs || !block_id || !dst || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false;

	if ((lseek(bs->fd, (block_id * BLOCK_SIZE), SEEK_SET) >= 0)) {
		if (read(bs->fd, dst, BLOCK_SIZE) > -1 ) {
			return true;
		} else {
		    printf("\nValue of errno: %d\n", errno);
		    printf("Error opening file: %s\n\n", strerror( errno ));
		}
	}
	return false;
}

bool back_store_write(back_store_t *const bs, const unsigned block_id, const void *const src) {
	if (!bs || !block_id || !src || block_id < NUM_FBM_BLOCKS || block_id > NUM_BLOCKS-1) return false;
	if (lseek(bs->fd, (off_t)(block_id * BLOCK_SIZE), SEEK_SET) >= 0) {
		if (write(bs->fd, src, BLOCK_SIZE) > -1 ) {
			return true;
		}
	}
	return false;
}
