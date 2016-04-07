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
	back_store_close(fs->bs);

	// free filesystem
	free(fs);

	return 0;
}

int fs_create(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) {
		printf("\nBad Parameters for fs_create");
		return -1;
	}

	traversal_results_t traverse = tree_traversal(fs, path);
	if (traverse.error_code != 0) {
		printf("\nTRAVERSE FAILED!!!");
		return -1;
	} else {
		printf("\nTraverse worked!!!");
	}

	if (type == FS_DIRECTORY) {
		int open_inode = find_open_inode(fs);
		if (open_inode < 1) {
			printf("\nError: Out of inodes");
			return -1; // < 1 because a return of 0 would be the root inode, which is an error
		}
		// fill the first dataptr with an empty directory_block
		block_ptr_t root_block = back_store_allocate(fs->bs);
		dir_block_t *root_dir = (dir_block_t *)calloc(1, sizeof(dir_block_t));
		root_dir->mdata.type = FS_DIRECTORY;
		if (back_store_write(fs->bs, root_block, (void *)root_dir) == false) return false;

		dir_block_t temp;
		if (back_store_read(fs->bs, traverse.parent_directory.data_ptrs[0], &temp) == false) {
			printf("FS_Create error: Could not read directory");
			return -1;
		}

		bool added = get_open_entry(temp);

		if (added == false) {
			printf("FS_Create error: NO room in the directory to add file!");
			free(root_dir);
			return -1;
		} else {
			if (back_store_write(fs->bs, traverse.parent_directory.data_ptrs[0], (void *)&(temp)) == false) {
				free(root_dir);
				printf("Could not add file to directory");
				return -1;
			}
		}

		fs->inode_array[open_inode] = (inode_t){"",{0, FS_DIRECTORY, {}}, {root_block}};
		strcpy(fs->inode_array[open_inode].fname,traverse.fname);

		free(root_dir);
	} else {
		int open_inode = find_open_inode(fs);
		if (open_inode < 1) {
			printf("\nError: Out of inodes");
			return -1; // < 1 because a return of 0 would be the root inode, which is an error
		}

		// load parent dir
		dir_block_t temp;
		if (back_store_read(fs->bs, traverse.parent_directory.data_ptrs[0], &temp) == false) {
			printf("FS_Create error: Could not read directory");
			return -1;
		}

		// add the inode # to the parent dir
		bool added = false;
		for (int i=0; i<DIR_REC_MAX; ++i) {

			if (strlen(temp.entries[i].fname) == 0) {
				strcpy(temp.entries[i].fname, traverse.fname);
				temp.entries[i].inode = open_inode;
				added = true;
				break;
			}
		}

		if (added == false) {
			printf("FS_Create error: NO room in the directory to add file!");
			return -1;
		} else {
			if (back_store_write(fs->bs, traverse.parent_directory.data_ptrs[0], (void *)&(temp)) == false) {
				printf("Could not add file to directory");
				return -1;
			}
		}
		//write dir block to BS

		fs->inode_array[open_inode] = (inode_t){"0",{0, FS_REGULAR, {}}, {}};
		strcpy(fs->inode_array[open_inode].fname,traverse.fname);
	}

	if (write_inode_array_to_backstore(fs) == false) return -1;
	// set the filename, MD, and block pointers of the inode

	return 0;
}

//// HELPER FUNCTIONS ////

bool format_inode_table(S16FS_t *fs) {
	if (!fs) return false;

	fs->inode_array = (inode_t *)calloc(256, sizeof(inode_t));
	if(!(fs->inode_array)) return false;

	// requests all the blocks in the BS for the inode table
	for (int block=8; block<40; ++block) { // Have to start at 8 because blocks 0-7 are the FBM in the backstore 	
		if (back_store_request(fs->bs, block) != true) {
			free(fs->inode_array);
			return false;
		}
	}

	block_ptr_t root_block = back_store_allocate(fs->bs);
	fs->inode_array[0] = (inode_t){"/",{0, FS_DIRECTORY, {}}, {root_block}};
	if (write_inode_array_to_backstore(fs) == false) {
		free(fs->inode_array);
		return false;
	}

	dir_block_t *root_dir = (dir_block_t *)calloc(1, sizeof(dir_block_t));
	root_dir->mdata.type = FS_DIRECTORY;
	if (back_store_write(fs->bs, root_block, (void *)root_dir) == false) {
		free(fs->inode_array);
		return false;
	}
	free(root_dir);

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

	if (*path != '/') {
		printf("\nTraverse Error: Path must start with '/' character");
		results.error_code = -1;
		return results;
	}

	if (*(path + strlen(path)-1) == '/') {
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
		token = strtok(NULL, "/");

		if (token != NULL) {

			// printf("\n\nSIZE: %d", strlen(token));
			// printf("\n%s\n", token);
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

			for (int i = 0; i < DIR_REC_MAX; ++i) {
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

bool get_open_entry(dir_block_t temp) {
	for (int i=0; i<DIR_REC_MAX; ++i) {

		if (strlen(temp.entries[i].fname) == 0) {
			strcpy(temp.entries[i].fname, traverse.fname);
			temp.entries[i].inode = open_inode;
			return true;
		}
	}
	return false;
}
