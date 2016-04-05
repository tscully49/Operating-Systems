#include "S16FS.h"

// metadata, inode, S16FS_t ---> padding at end???

// directory, directory block, file descriptor tables ---> From design 

typedef struct {
	uint32_t size;
	uint8_t type;
	uint8_t padding[43];
} mdata_t;

typedef struct {
	char fname[FS_FNAME_MAX];
	mdata_t mdata;
	//block_ptr_t data_ptrs[8];
} inode_t;

typedef struct {
	char fname[FS_FNAME_MAX];
	//inode_ptr_t inode;
} dir_ent_t;

typedef struct {
	mdata_t mdata;
	dir_ent_t entries[DIR_REC_MAX];
	uint8_t padding;
} dir_block_t;

typedef struct {
	bitmap_t *fd_status;
	size_t fd_pos[DESCRIPTOR_MAX];
	//inode_ptr_t fd_inode[DESCRIPTOR_MAX];
} fd_table_t;

struct S16FS {
	back_store_t *bs;
	fd_table_t fd_table;
};

S16FS_t *fs_format(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL;

	// ???? Did I typecast this properly???
	uint8_t *inode_table = (uint8_t *)calloc(8, sizeof(inode_t)); // ???? WHAT SHOULD the inode table actually be???
	if (!inode_table) return NULL;

	S16FS_t *filesystem = (S16FS_t *)malloc(sizeof(S16FS_t));
	if (!filesystem) return NULL;

	filesystem->bs = back_store_create(path); // ???? DOES THIS need to be parsed??? 
	if (filesystem->bs == NULL) return NULL;

	for (int i=8; i<40; ++i) { // Have to start at 8 because blocks 0-7 are the FBM in the backstore 
		// write to backstore
		if (back_store_request(filesystem->bs, i) == true) {
			if (back_store_write(filesystem->bs, i, inode_table) != true) {
				return NULL;
			}
		} else {
			return NULL;
		}
	}

	// update FBM

	// set first index to the root

	// write blank to the rest of the inodes (didn't I do this already???)

	// mount FS 

	return NULL;
}

S16FS_t *fs_mount(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL;



	return NULL;
}

int fs_unmount(S16FS_t *fs) {
	if (!fs) return -1;

	return -1;
}

int fs_create(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0 || !type) return -1;

	return -1;
}
