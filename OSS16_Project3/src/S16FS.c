#include "S16FS.h"
#include <stdio.h>
#include <string.h>

// metadata, inode, S16FS_t ---> padding at end???

// directory, directory block, file descriptor tables ---> From design 

struct S16FS {
	back_store_t *bs;
	fd_table_t fd_table;
	inode_t *inode_array;
	int root_inode_number;
};

bool write_inode_array_to_backstore(S16FS_t *fs);
bool format_inode_table(S16FS_t *fs);
bool read_inode_array_from_backstore(S16FS_t *fs);
int find_open_inode(S16FS_t *fs);
traversal_results_t* tree_traversal(S16FS_t *fs, const char *path, file_t type);

S16FS_t *fs_format(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL;

	S16FS_t *fs = (S16FS_t *)calloc(1, sizeof(S16FS_t));
	if (!fs) return NULL;

	fs->bs = back_store_create(path);
	if (fs->bs == NULL) return NULL;

	if (format_inode_table(fs) == false) return NULL;

	return fs;
}

S16FS_t *fs_mount(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL;

	S16FS_t *fs = (S16FS_t *)malloc(sizeof(S16FS_t));
	if (!fs) return NULL;

	fs->bs = back_store_open(path);
	if (fs->bs == NULL) return NULL;

	if (read_inode_array_from_backstore(fs) == false) return NULL;

	return fs;
}

int fs_unmount(S16FS_t *fs) {
	if (!fs) return -1;

	// write inode array to BS
	if (write_inode_array_to_backstore(fs) != true) return -1;
	// free inode array
	free(fs->inode_array);
	// free backstore
	free(fs->bs);

	// free filesystem
	free(fs);

	return 0;
}

int fs_create(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return -1;

	// find open inode
	int open_inode = find_open_inode(fs);
	if (open_inode < 1) return -1; // < 1 because a return of 0 would be the root inode, which is an error

	traversal_results_t *traverse = tree_traversal(fs, path, type);
	if (traverse == NULL) return -1;

	if (type == FS_DIRECTORY) return 0;

	// (fs->inode_array + open_inode)->fname = ;
	// (fs->inode_array + open_inode)->mdata = (mdata_t *)calloc(1, sizeof(mdata_t));
	// if ((fs->inode_array + open_inode)->mdata == NULL) return -1;
	// // fill the metadata 
	// (fs->inode_array + open_inode)->fname = ;

	// set the filename, MD, and block pointers of the inode



	return -1;
}

//// HELPER FUNCTIONS ////

bool format_inode_table(S16FS_t *fs) {
	if (!fs) return false;

	fs->inode_array = (inode_t *)calloc(256, sizeof(inode_t));
	if(!(fs->inode_array)) return false;

	// requests all the blocks in the BS for the inode table
	for (int block=8; block<40; ++block) { // Have to start at 8 because blocks 0-7 are the FBM in the backstore 	
		if (back_store_request(fs->bs, block) != true) return false;
	}

	block_ptr_t root_block = back_store_allocate(fs->bs);
	fs->inode_array[0] = (inode_t){"/",{0, FS_DIRECTORY, {}}, {root_block}};
	if (write_inode_array_to_backstore(fs) == false) return false;
	fs->root_inode_number = root_block;

	return true;
}

bool write_inode_array_to_backstore(S16FS_t *fs) {
	if (!fs) return false; 

	for (int i=0, block=8; i<256; i+=8, ++block) {
		if (back_store_write(fs->bs, block, (void *)(fs->inode_array + i)) != true) {
		 	printf("\nWRITE FAILED\n");
		 	return false;
		}
	}
	return true;
}

bool read_inode_array_from_backstore(S16FS_t *fs) {
	if (!fs) return false;

	fs->inode_array = (inode_t *)calloc(256, sizeof(inode_t));
	if(!(fs->inode_array)) return false;

	for (int i=0, block=8; i<256; i+=8, ++block) {
		if (back_store_read(fs->bs, block, (fs->inode_array + i)) != true) {
		 	printf("\nREAD FAILED\n");
		 	return false;
		}
	}
	return true;
}

int find_open_inode(S16FS_t *fs) {
	if (!fs) return -1;

	inode_t *intbl = fs->inode_array;
	for (int i = 0; i < DESCRIPTOR_MAX; ++i) {
		if (!*(intbl + i)->fname) {
			printf("\n\n%d\n\n", i);
			return i;
		}
	}

	return -1;
}

traversal_results_t* tree_traversal(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0 ) return NULL;

	inode_t *dir = fs->inode_array;

	const size_t str_length = strlen(path);
	char *new_path = strndup(path, str_length);
	char *token;
	token = strtok(new_path, "/");

	while (token != NULL) {
		printf("\n%s\n", token);

		// scan current directory for filenam
		if () { // if not found

		} else { // if found
			if () { // file type is regular AND there is NOT more tokenizing to do

			} else if () { // if file type is regular AND there IS more tokenizing to do 

			} else if () { // if file type is directory AND there IS more tokenizing to do

			} else { // else unknown error

			}
		}

		token = strtok(NULL, "/");
	}

	return NULL;
}
