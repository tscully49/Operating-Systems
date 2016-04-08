#include "S16FS.h"
#include <stdio.h>
#include <string.h>

// metadata, inode, S16FS_t ---> padding at end???

// directory, directory block, file descriptor tables ---> From design 

struct S16FS {
	back_store_t *bs;
	fd_table_t fd_table;
	inode_t *inode_array;
};

bool write_inode_array_to_backstore(S16FS_t *fs);
bool format_inode_table(S16FS_t *fs);
bool read_inode_array_from_backstore(S16FS_t *fs);
int find_open_inode(S16FS_t *fs);
traversal_results_t tree_traversal(S16FS_t *fs, const char *path);
bool add_inode_to_parent_dir(S16FS_t *fs, dir_block_t temp, traversal_results_t traverse, uint8_t type, int open_inode, block_ptr_t root_block);

S16FS_t *fs_format(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL; // param check

	S16FS_t *fs = (S16FS_t *)calloc(1, sizeof(S16FS_t)); // find memory and initialize for the filesystem object
	if (!fs) return NULL;

	fs->bs = back_store_create(path); // create the back store
	if (fs->bs == NULL) return NULL;

	if (format_inode_table(fs) == false) return NULL; // call function to format the inode table

	return fs; // return the file system
}

S16FS_t *fs_mount(const char *path) {
	if (!path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL; // param check 

	S16FS_t *fs = (S16FS_t *)malloc(sizeof(S16FS_t)); // get memory for the file system structure but don't initialize it
	if (!fs) return NULL;

	fs->bs = back_store_open(path); // open the back store and load to the backstore
	if (fs->bs == NULL) return NULL;

	if (read_inode_array_from_backstore(fs) == false) return NULL; // read the inode array from the backstore 

	return fs; // return the file system
}

int fs_unmount(S16FS_t *fs) {
	if (!fs) return -1; // param check

	// write inode array to BS
	if (write_inode_array_to_backstore(fs) != true) return -1;
	// free inode array
	free(fs->inode_array);
	// free backstore
	back_store_close(fs->bs);

	// free filesystem
	free(fs);

	return 0; // return 0 if no errors
}

int fs_create(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) { // check params
		printf("\nBad Parameters for fs_create");
		return -1;
	}

	traversal_results_t traverse = tree_traversal(fs, path); // traverse the file path and get data from it
	if (traverse.error_code != 0) { // check if the traversal worked or not 
		printf("\nTRAVERSE FAILED!!!");
		return -1;
	} else {
		printf("\nTraverse worked!!!");
	}

	if (type == FS_DIRECTORY) { // if the file to be created was a directory or not
		int open_inode = find_open_inode(fs); // find an open inode
		if (open_inode < 1) { // make sure the inode is not the root
			printf("\nError: Out of inodes");
			return -1; // < 1 because a return of 0 would be the root inode, which is an error
		}
		// fill the first dataptr with an empty directory_block
		block_ptr_t root_block = back_store_allocate(fs->bs); // allocate a block for the directory
		dir_block_t *root_dir = (dir_block_t *)calloc(1, sizeof(dir_block_t)); // find memory for the directory
		root_dir->mdata.type = FS_DIRECTORY; // set the metadata type to directory
		if (back_store_write(fs->bs, root_block, (void *)root_dir) == false) return false; // write the block to the back store
		free(root_dir);

		dir_block_t temp; // create temp variable to hold the parent directory to add the directory to 
		if (back_store_read(fs->bs, traverse.parent_directory.data_ptrs[0], &temp) == false) { // read the data from back store 
			printf("FS_Create error: Could not read directory");
			return -1;
		}

		bool added = add_inode_to_parent_dir(fs, temp, traverse, FS_DIRECTORY, open_inode, root_block); // add the directory to the directory
		if (added == false) return -1; // check to make sure that the directory was added 

	} else {
		int open_inode = find_open_inode(fs); // find an open inode
		if (open_inode < 1) { // check that the inode is not the root
			printf("\nError: Out of inodes");
			return -1; // < 1 because a return of 0 would be the root inode, which is an error
		}

		// load parent dir
		dir_block_t temp; // create temp variable to hold parent inode
		if (back_store_read(fs->bs, traverse.parent_directory.data_ptrs[0], &temp) == false) { // read data from the back store
			printf("FS_Create error: Could not read directory"); 
			return -1;
		}

		bool added = add_inode_to_parent_dir(fs, temp, traverse, FS_REGULAR, open_inode, 0); // add file to the parent directory
		if (added == false) return -1; // check to make sure that the file was added 

	}

	if (write_inode_array_to_backstore(fs) == false) return -1; // write the inode array to the back store
	// set the filename, MD, and block pointers of the inode

	return 0;
}

//// HELPER FUNCTIONS ////

bool format_inode_table(S16FS_t *fs) {
	if (!fs) return false; // param check

	fs->inode_array = (inode_t *)calloc(256, sizeof(inode_t)); // calloc memory for the inode array
	if(!(fs->inode_array)) return false; // if the inode array has memory 

	// requests all the blocks in the BS for the inode table
	for (int block=8; block<40; ++block) { // Have to start at 8 because blocks 0-7 are the FBM in the backstore 	
		if (back_store_request(fs->bs, block) != true) { // request blocks for the inode array
			free(fs->inode_array);
			return false;
		}
	}

	block_ptr_t root_block = back_store_allocate(fs->bs); // allocate a block for the root directory
	fs->inode_array[0] = (inode_t){"/",{0, FS_DIRECTORY, {}}, {root_block}}; // create the inode root
	if (write_inode_array_to_backstore(fs) == false) { // write the root inode to the back store
		free(fs->inode_array);
		return false;
	}

	dir_block_t *root_dir = (dir_block_t *)calloc(1, sizeof(dir_block_t)); // calloc memory for the root directory
	root_dir->mdata.type = FS_DIRECTORY; // set to directory
	if (back_store_write(fs->bs, root_block, (void *)root_dir) == false) { // write root directory to the back store 
		free(fs->inode_array);
		return false;
	}
	free(root_dir);

	return true;
}

bool write_inode_array_to_backstore(S16FS_t *fs) {
	if (!fs) return false; 

	for (int i=0, block=8; i<256; i+=8, ++block) {
		if (back_store_write(fs->bs, block, (void *)(fs->inode_array + i)) != true) { // write to back store 
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
		if (back_store_read(fs->bs, block, (fs->inode_array + i)) != true) { // read inode array from back store
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
			return i;
		}
	}

	return -1;
}

traversal_results_t tree_traversal(S16FS_t *fs, const char *path) {
	// Set default return structure values
	traversal_results_t results = {{}, {}, "", 0}; // set to the root
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) {
		printf("\nTraverse Error: Parameter Error");
		results.error_code = -1;
		return results;
	}

	if (*path != '/') { // check that the beginning is the root directory
		printf("\nTraverse Error: Path must start with '/' character");
		results.error_code = -1;
		return results;
	}

	if (*(path + strlen(path)-1) == '/') { // check that the end is not a / 
		printf("\nTraverse Error: Path cannot end with a '/' character");
		results.error_code = -1;
		return results;
	}

	results.parent_directory = *fs->inode_array;

	// Declare current directory
	dir_block_t dir;
	if (back_store_read(fs->bs, results.parent_directory.data_ptrs[0], &dir) == false) {
		printf("\nTraverse Error: read to first directory failed");
		results.error_code = -1;
		return results;
	}

	const size_t str_length = strlen(path); // the length of the string to duplicate
	char *new_path = strndup(path, str_length); // duplicated string casting from const char* to char*
	

	char *token; // the "next" token
	char *previous_token; // the directory that we were just at
	inode_ptr_t current_inode_index; // the inode index if found in the directory block
	uint8_t file_type = dir.mdata.type;
	token = strtok(new_path, "/"); // get the first token

	if (token) {
		if (strlen(token) >= 64) {
			printf("\nTraverse Error: Filename too long ---> %s\n", token);
			results.error_code = 3;
			free (new_path);
			return results;
		}
	}

	while (token != NULL) {

		previous_token = token; 
		token = strtok(NULL, "/"); // check that there is another path to search for 

		if (token != NULL) {

			// Make sure that the next token is within the 64 character constraint
			if (strlen(token) >= 64) {
				printf("\nTraverse Error: Filename too long ---> %s\n", token);
				results.error_code = 3;
				free(new_path);
				return results;
			}

			// Search for filename in the current directory's directory block
			bool found = false;

			// for loop through directory to search for filename in current directory
			for (int i = 0; i < DIR_REC_MAX; ++i) {
				//printf("\nI: %d\nPREV: %s\nDIR: %s\n", i, previous_token, dir.entries[i].fname);
				if (strcmp(previous_token, dir.entries[i].fname) == 0) {
					found = true;
					current_inode_index = dir.entries[i].inode;
					break;
				}
			}

			if (found == false) { // if the file was not found
				printf("\nError in traversing tree:  File not Found in the Current Directory");
				results.error_code = 1;
				free(new_path);
				return results;
			} else { // if the file was found 
				if (file_type == FS_DIRECTORY) { // if file type is directory AND there IS more tokenizing to do
					// load current directory to
					if (back_store_read(fs->bs, (fs->inode_array + current_inode_index)->data_ptrs[0], &dir) == false) {
						printf("\nTraverse Error: read to next directory failed");
						results.error_code = -1;
						free(new_path);
						return results;
					}

					results.parent_directory = *(fs->inode_array + current_inode_index);

					file_type = dir.mdata.type;
				} else if (file_type == FS_REGULAR) { // if file type is regular AND there IS more tokenizing to do 
					printf("\nError in Traversing File: Reached a file but not done traversing tree");
					results.error_code = 1;
					free(new_path);
					return results;
				}
			}
		} else { // 
			bool found = false;

			for (int i = 0; i < DIR_REC_MAX; ++i) { // search for the file 
				if (strcmp(previous_token, dir.entries[i].fname) == 0) { 
					found = true;
					current_inode_index = i;
					break;
				}
			}

			if (found != false) { // if the file was not found 
				printf("Traverse Tree Error:  File/Directory Already exists");
				results.error_code = 1;
				free(new_path);
				return results;
			} else {
				if (file_type == FS_REGULAR) { // file type is regular AND there is NOT more tokenizing to do
					// return the current inode number and the parent inode number
					strcpy(results.fname,previous_token);
					return results;
				} else { // else unknown error
					//printf("\nUnknown error in traversing file tree\n");
					//results.error_code = -1;
					strcpy(results.fname,previous_token);
					free(new_path);
					return results;
				}
			}
		}
	}
	free(new_path);

	printf("\nUnexpected Error in traversing tree, reached end of function...");
	results.error_code = -1;
	return results;
}

bool add_inode_to_parent_dir(S16FS_t *fs, dir_block_t temp, traversal_results_t traverse, uint8_t type, int open_inode, block_ptr_t root_block) {
	bool added = false; 
	for (int i=0; i<DIR_REC_MAX; ++i) { // check to see if the directory has an open spot 

		if (strlen(temp.entries[i].fname) == 0) { 
			strcpy(temp.entries[i].fname, traverse.fname);
			temp.entries[i].inode = open_inode;
			added = true; 
			break;
		}
	}

	if (added == false) {
		printf("FS_Create error: NO room in the directory to add file!");
		return false;
	} else { 
		if (back_store_write(fs->bs, traverse.parent_directory.data_ptrs[0], (void *)&(temp)) == false) { // write the data block of the parent directory
			printf("Could not add file to directory");
			return false;
		}
	}
	if (type == FS_DIRECTORY) {
		fs->inode_array[open_inode] = (inode_t){"",{0, type, {}}, {root_block}}; // create the inode for the directory
	} else {
		fs->inode_array[open_inode] = (inode_t){"",{0, type, {}}, {}}; // create the inode for the file 
	}
	strcpy(fs->inode_array[open_inode].fname,traverse.fname);
	return true;
}
