#include <back_store.h>
#include <bitmap.h>

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "S16FS.h"

// There's just so much. SO. MUCH.
// Last time it was all in the file.
// There was just so much
// It messes up my autocomplete, but whatever.
#include "backend.h"

///
/// Formats (and mounts) an S16FS file for use
/// \param fname The file to format
/// \return Mounted S16FS object, NULL on error
///
S16FS_t *fs_format(const char *path) {
    return ready_file(path, true);
}

///
/// Mounts an S16FS object and prepares it for use
/// \param fname The file to mount
/// \return Mounted F16FS object, NULL on error
///
S16FS_t *fs_mount(const char *path) {
    return ready_file(path, false);
}

///
/// Unmounts the given object and frees all related resources
/// \param fs The S16FS object to unmount
/// \return 0 on success, < 0 on failure
///
int fs_unmount(S16FS_t *fs) {
    if (fs) {
        back_store_close(fs->bs);
        bitmap_destroy(fs->fd_table.fd_status);
        free(fs);
        return 0;
    }
    return -1;
}

///
/// Creates a new file at the specified location
///   Directories along the path that do not exist are not created
/// \param fs The S16FS containing the file
/// \param path Absolute path to file to create
/// \param type Type of file to create (regular/directory)
/// \return 0 on success, < 0 on failure
///
int fs_create(S16FS_t *fs, const char *path, file_t type) {
    if (fs && path) {
        if (type == FS_REGULAR || type == FS_DIRECTORY) {
            // WHOOPS. Should make sure desired file doesn't already exist.
            // Just going to jam it here.
            result_t file_status;
            locate_file(fs, path, &file_status);
            if (file_status.success && !file_status.found) {
                // alrighty. Need to find the file. And by the file I mean the parent.
                // locate_file doesn't really handle finding the parent if the file doesn't exist
                // So I can't just dump finding this file. Have to search for parent.

                // So, kick off the file finder. If it comes back with the right flags
                // Start checking if we have inodes, the parent exists, a directory, not full
                // if it's a dir check if we have a free block.
                // Fill it all out, update parent, etc. Done!
                const size_t path_len = strnlen(path, FS_PATH_MAX);
                if (path_len != 0 && path[0] == '/' && path_len < FS_PATH_MAX) {
                    // path string is probably ok.
                    char *path_copy, *fname_copy;
                    // this breaks if it's a file at root, since we remove the slash
                    // locate_file treats it as an error
                    // Old version just worked around if if [0] was '\0'
                    // Ideally, I could just ask strndup to allocate an extra byte
                    // Then I can just shift the fname down a byte and insert the NUL there
                    // But strndup doesn't allocate the size given, it seems
                    // So we gotta go manual. Don't think this snippet will be needed elsewhere
                    // Need a malloc, memcpy, then some manual adjustment
                    // path_copy  = strndup(path, path_len);  // I checked, it's not +1. yay MallocScribble
                    path_copy = (char *) calloc(1, path_len + 2);  // NUL AND extra space
                    memcpy(path_copy, path, path_len);
                    fname_copy = strrchr(path_copy, '/');
                    if (fname_copy) {  // CANNOT be null, since we validated [0] as a possibility... but just in case
                        //*fname_copy = '\0';  // heh, split strings, now I have a path to parent AND fname
                        ++fname_copy;
                        const size_t fname_len = path_len - (fname_copy - path_copy);
                        memmove(fname_copy + 1, fname_copy, fname_len + 1);
                        fname_copy[0] = '\0';  // string is split into abs path (now with slash...) and fname
                        ++fname_copy;

                        if (fname_len != 0 && fname_len < (FS_FNAME_MAX - 1)) {
                            // alrighty. Hunt down parent dir.
                            // check it's actually a dir. (ooh, add to result_t!)
                            locate_file(fs, path_copy, &file_status);
                            if (file_status.success && file_status.found && file_status.type == FS_DIRECTORY) {
                                // parent exists, is a directory. Cool.
                                // (added block to locate_file if file is a dir. Handy.)
                                dir_block_t parent_dir;
                                inode_t new_inode;
                                dir_block_t new_dir;
                                uint32_t now = time(NULL);
                                // load dir, check it has space.
                                if (full_read(fs, &parent_dir, file_status.block)
                                    && parent_dir.mdata.size < DIR_REC_MAX) {
                                    // try to grab all new resources (inode, optionally data block)
                                    // if we get all that, commit it.
                                    inode_ptr_t new_inode_idx = find_free_inode(fs);
                                    if (new_inode_idx != 0) {
                                        bool success            = false;
                                        block_ptr_t new_dir_ptr = 0;
                                        switch (type) {
                                            case FS_REGULAR:
                                                // We're all good.
                                                new_inode = (inode_t){
                                                    {0},
                                                    {0, 0777, now, now, now, file_status.inode, FS_REGULAR, {0}},
                                                    {0}};
                                                strncpy(new_inode.fname, fname_copy, fname_len + 1);
                                                // I'm so deep now that my formatter is very upset with every line
                                                // inode = ready
                                                success = write_inode(fs, &new_inode, new_inode_idx);
                                                // Uhh, if that didn't work we could, worst case, have a partial inode
                                                // And that's a "file system is now kinda busted" sort of error
                                                // This is why "real" (read: modern) file systems have backups all over
                                                // (and why the occasional chkdsk is so important)
                                                break;
                                            case FS_DIRECTORY:
                                                // following line keeps being all "Expected expression"
                                                // SOMETHING is messed up SOMEWHERE.
                                                // Or it's trying to protect me by preventing new variables in a switch
                                                // Which is super undefined, but only sometimes (not in this case...)
                                                // Idk, man.
                                                // block_ptr_t new_dir_ptr = back_store_allocate(fs->bs);
                                                new_dir_ptr = back_store_allocate(fs->bs);
                                                if (new_dir_ptr != 0) {
                                                    // Resources = obtained
                                                    // write dir block first, inode is the final step
                                                    // that's more transaction-safe... but it's not like we're thread
                                                    // safe
                                                    // in the slightest (or process safe, for that matter)
                                                    new_inode = (inode_t){
                                                        {0},
                                                        {0, 0777, now, now, now, file_status.inode, FS_DIRECTORY, {0}},
                                                        {new_dir_ptr, 0, 0, 0, 0, 0}};
                                                    strncpy(new_inode.fname, fname_copy, fname_len + 1);

                                                    memset(&new_dir, 0x00, sizeof(dir_block_t));

                                                    if (!(success = full_write(fs, &new_dir, new_dir_ptr)
                                                                    && write_inode(fs, &new_inode, new_inode_idx))) {
                                                        // transation: if it didn't work, release the allocated block
                                                        back_store_release(fs->bs, new_dir_ptr);
                                                    }
                                                }
                                                break;
                                            default:
                                                // HOW.
                                                break;
                                        }
                                        if (success) {
                                            // whoops. forgot the part where I actually save the file to the dir tree
                                            // Mildly important.
                                            unsigned i = 0;
                                            // This is technically a potential infinite loop. But we validated contents
                                            // earlier
                                            for (; parent_dir.entries[i].fname[0] != '\0'; ++i) {
                                            }
                                            strncpy(parent_dir.entries[i].fname, fname_copy, fname_len + 1);
                                            parent_dir.entries[i].inode = new_inode_idx;
                                            ++parent_dir.mdata.size;
                                            if (full_write(fs, &parent_dir, file_status.block)) {
                                                free(path_copy);
                                                return 0;
                                            } else {
                                                // Oh man. These surely are the end times.
                                                // Our file exists. Kinda. But not entirely.
                                                // The final tree link failed.
                                                // We SHOULD:
                                                //  Wipe inode
                                                //  Release dir block (if making a dir)
                                                // But I'm lazy. And if a write failed, why would others work?
                                                // back_store won't actually do that to us, anyway.
                                                // Like, even if the file was deleted while using it, we're mmap'd so
                                                // the kernel has no real way to tell us, as far as I know.
                                                puts("Infinite sadness. New file stuck in limbo.");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        free(path_copy);
                    }
                }
            }
        }
    }
    return -1;
}

///
/// Opens the specified file for use
///   R/W position is set to the beginning of the file (BOF)
///   Directories cannot be opened
/// \param fs The S16FS containing the file
/// \param path path to the requested file
/// \return file descriptor to the requested file, < 0 on error
///
int fs_open(S16FS_t *fs, const char *path) {
    if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) { // check params
        printf("\nBad Parameters for fs_open");
        return -1;
    }
    
    result_t file_status;
    locate_file(fs, path, &file_status);

    if (file_status.success) {
    	if (file_status.found) {
    		if (file_status.type == FS_REGULAR) {
    			printf("\nThis is a good file!!!");
    			// Find open inode
    			size_t open_fd = bitmap_ffz(fs->fd_table.fd_status);
    			if (open_fd == SIZE_MAX) {
    				printf("\nError with fs_open: out of available fd numbers");
    				return -1;
    			} else {
    				// Fill with metadata
    				bitmap_set(fs->fd_table.fd_status, open_fd);
    				fs->fd_table.fd_inode[open_fd] = file_status.inode;
    				fs->fd_table.fd_pos[open_fd] = 0;
    				// Return with the fd_table index as the fd
    				return open_fd;
    			} 
    		} else {
    			printf("\nError with fs_open: the inode found is a directory, not a file");
    			return -1;
    		}
    	} else {
    		printf("\nError with fs_open: the file was not found from locate_file, but the function returned successfully");
    		return -1;
    	}
    } else {
    	printf("\nError with fs_open: locate_file function ran into an error");
    	return -1;
    }

    return -1;
}

///
/// Closes the given file descriptor
/// \param fs The S16FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(S16FS_t *fs, int fd) {
    if (!fs || fd < 0) {
        printf("\nBad Parameters for fs_close");
        return -1;
    }

    if (bitmap_test(fs->fd_table.fd_status, fd) == false) {
    	printf("\nfs_close Error: Cannot close that fd, the fd is not currently in use");
    	return -1;
    } else {
    	bitmap_reset(fs->fd_table.fd_status, fd);
		return 0;
    }

    return -1;
}

///
/// Writes data from given buffer to the file linked to the descriptor
///   Writing past EOF extends the file
///   Writing inside a file overwrites existing data
///   R/W position in incremented by the number of bytes written
/// \param fs The S16FS containing the file
/// \param fd The file to write to
/// \param dst The buffer to read from
/// \param nbyte The number of bytes to write
/// \return number of bytes written (< nbyte IFF out of space), < 0 on error
///
ssize_t fs_write(S16FS_t *fs, int fd, const void *src, size_t nbyte) {
	//printf("\nSTARTED\n");
    if (!fs || !src || nbyte > FILE_SIZE_MAX || fd >= DESCRIPTOR_MAX || fd < 0) {
        printf("\nBad Parameters for fs_write");
        return -1;
    }

    if (nbyte == 0) return 0;

    if (bitmap_test(fs->fd_table.fd_status, fd) == false) {
    	printf("\nFD not active for fs_write");
    	return -1;
    }

    //printf("\nNEW ONE: %zu", fs->fd_table.fd_pos[fd]);

    size_t num_blocks_to_write;
    if (fs->fd_table.fd_pos[fd]%BLOCK_SIZE != 0) {
    	size_t temp_num = nbyte - (fs->fd_table.fd_pos[fd] - (fs->fd_table.fd_pos[fd]/BLOCK_SIZE));
    	num_blocks_to_write = (temp_num / BLOCK_SIZE) + 1;
    	if (temp_num % BLOCK_SIZE != 0) num_blocks_to_write++;
    } else {
    	num_blocks_to_write = nbyte / BLOCK_SIZE; // finds the number of blocks needed to write to the inode
    	if (nbyte % BLOCK_SIZE != 0) num_blocks_to_write++; // if not perfectly divisible, you will need an extra block
    }

    inode_t file_inode;
    if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd]) == false) {
    	printf("\nError with fs_write: Could not read inode of file from fd");
    	return -1;
    }

    size_t fd_pos = fs->fd_table.fd_pos[fd]; // current 
    //printf("\nGot to end of fs_Write and got %zu", fs->fd_table.fd_pos[fd]);
    size_t fd_pos_block = fd_pos/BLOCK_SIZE; // don't add 1 if not a clean division
    size_t blocks_written = 0;
    size_t bytes_written = 0;

    if (fd_pos + nbyte > FILE_SIZE_MAX) {
    	printf("\nError with fs_write: Trying to extend file past the maximum file size");
    	return -1;
    }

    // build dyn array of data blocks written to 
    dyn_array_t *data_blocks_written_to = build_data_ptrs_array(fs, num_blocks_to_write, src, fd, nbyte, &bytes_written, &blocks_written);
    if (data_blocks_written_to == NULL) {
    	printf("\nError with build_data_ptrs_array function");
    	return -1;
    }

    //printf("\nNEW BYTES WRITTEN: %zu", bytes_written);

    //printf("\nBytes written but not added to inode: %zu\n", bytes_written);

    size_t total_blocks_in_file = fd_pos_block+blocks_written;
    // iterate through dyn_array and update the inode block pointers 

    //printf("\nERROR CHECK, TOTAL: %zu, FD_POS_BLOCK: %zu\n", total_blocks_in_file, fd_pos_block);

    if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd]) == false) {
    	printf("\nError with fs_write: Could not read inode of file from fd");
    	dyn_array_destroy(data_blocks_written_to);
    	return -1;
    }

    while (fd_pos_block < total_blocks_in_file) {
    	block_ptr_t temp;
    	if (dyn_array_extract_front(data_blocks_written_to, &temp) != true) {
    		printf("\nfs_write error: cannot extract from dyn_array");
    		dyn_array_destroy(data_blocks_written_to);
    		return -1;
    	} 
    	// allocate the data block in the FBM
    	if (fd_pos_block <= 5) { // direct pointers
       		file_inode.data_ptrs[fd_pos_block] = temp;
    	} else if (fd_pos_block >= 6 && fd_pos_block < 518) { // indirect pointers
    		if (file_inode.data_ptrs[6] == 0) {
    			printf("\nERROR INDIR DOESNT EXIST");
    			dyn_array_destroy(data_blocks_written_to);
    			return -1;
     		} else {
     			//printf("\nFOUND INDIR");
    			// go to block and add to it
    			indir_block_t indirect_block;
    			if (full_read(fs, &indirect_block, file_inode.data_ptrs[6]) != true) {
    				printf("\nfs_write error: could not read indirectttt block from bs");
    				dyn_array_destroy(data_blocks_written_to);
    				return -1;
    			}

    			indirect_block.block_ptrs[fd_pos_block-6] = temp;

    			if (full_write(fs, (void *)&indirect_block, file_inode.data_ptrs[6]) != true) {
    				printf("\nfs_write error: could not write indirect block to bs");
    				dyn_array_destroy(data_blocks_written_to);
    				return -1;
    			}
    		}
    	} else if (fd_pos_block >= 518) { // double indirect pointers
    		if (file_inode.data_ptrs[7] == 0) {
    			printf("\nERROR DBL INDIR DOESNT EXIST...");
    			dyn_array_destroy(data_blocks_written_to);
    			return -1;
    		} else {
    			//printf("\nCHECK DBL: %zu", file_inode.data_ptrs[7]);
    			//printf("\nFOUND DBL INDIR");
    			// find which indirect pointer in the double indirect block we are going into
    			size_t double_indirect_index = (fd_pos_block-518)/(INDIRECT_TOTAL);

    			//printf("\nCHECK DBL INDEX: %zu, %zu", double_indirect_index, fd_pos_block);
    			indir_block_t double_indirect_block;
    			if (full_read(fs, &double_indirect_block, file_inode.data_ptrs[7]) != true) {
    				printf("\nfs_write error: could not read double indirect pointer");
    				dyn_array_destroy(data_blocks_written_to);
    				return -1;
    			}

    			if (double_indirect_block.block_ptrs[double_indirect_index] == 0) { // if doesn't
    				printf("\nERROR INDIR in DBL INDIR DOESNT EXIST...");
    				dyn_array_destroy(data_blocks_written_to);
    				return -1;
    			} else { // else 
    				//printf("\nFOUND INDIR IN DBL INDIR");
    				// find which index in that indirect block you are using
    				size_t indirect_index = (fd_pos_block-518) - (double_indirect_index*INDIRECT_TOTAL);
    				// set that index = temp 
    				indir_block_t indirect_block;
    				if (full_read(fs, &indirect_block, double_indirect_block.block_ptrs[double_indirect_index]) != true) {
	    				printf("\nfs_write error: could not read double indirect pointer");
	    				dyn_array_destroy(data_blocks_written_to);
	    				return -1;
	    			}

	    			indirect_block.block_ptrs[indirect_index] = temp;

	    			if (full_write(fs, (void *)&indirect_block, double_indirect_block.block_ptrs[double_indirect_index]) != true) {
	    				printf("\nfs_write error: could not write indirect block to bs");
	    				dyn_array_destroy(data_blocks_written_to);
	    				return -1;
	    			}
    			}
    		}
    	} else if (fd_pos_block >= 262662) {
    		printf("\nError with fs_write: Trying to extend file past the maximum file size");
    		dyn_array_destroy(data_blocks_written_to);
    		return -1;
    	}

    	fd_pos_block++;
    }

    if (dyn_array_empty(data_blocks_written_to) != true) {
    	printf("\nfs_write error: Dyn array not empty...  %zu", dyn_array_size(data_blocks_written_to));
    	dyn_array_destroy(data_blocks_written_to);
    	return -1;
    }

    dyn_array_destroy(data_blocks_written_to);

    // increase the size of the file and bump the size mdata of the file
    file_inode.mdata.size = fd_pos + nbyte;
    fs->fd_table.fd_pos[fd] = fd_pos + bytes_written;

    // write inode to BS ---> write_inode();
    if (write_inode(fs, (void *)&file_inode, fs->fd_table.fd_inode[fd]) != true) {
    	printf("\nfs_write error: Could not write inode to BS");
    	return -1;
    }

    //printf("\nGot to end of fs_Write and got %zu/%zu", fs->fd_table.fd_pos[fd], FILE_SIZE_MAX);
    //printf("\nFINISHED\n");
    return bytes_written;
}

///
/// Deletes the specified file and closes all open descriptors to the file
///   Directories can only be removed when empty
/// \param fs The S16FS containing the file
/// \param path Absolute path to file to remove
/// \return 0 on success, < 0 on error
///
int fs_remove(S16FS_t *fs, const char *path) {
    if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) {
        printf("\nBad Parameters for fs_remove");
        return -1;
    }

    result_t file_status;
    locate_file(fs, path, &file_status);

    if (file_status.success) {
    	if (file_status.found) {
    		if (file_status.type == FS_REGULAR) {
    			// reset bits in bitmap from FBM for the datablocks from the pointers in the inode
    				// need to make sure to clear the indirect and double indirect
    			dyn_array_t *data_blocks_in_file = build_array_of_file_data_ptrs(fs, file_status.inode);
    			if (data_blocks_in_file == NULL) {
    				printf("\nError in fs_remove: 'build array of file data ptrs' function failed");
    				dyn_array_destroy(data_blocks_in_file);
    				return -1;
    			}

    			for (int i=0; i<DESCRIPTOR_MAX; i++) {
    				if (bitmap_test(fs->fd_table.fd_status, i) == false) continue;
    				if (file_status.inode == fs->fd_table.fd_inode[i]) {
    					bitmap_reset(fs->fd_table.fd_status, i);
    					break;
    				}
    			}

    			while (dyn_array_empty(data_blocks_in_file) == false) {
    				block_ptr_t temp;
    				if (dyn_array_extract_back(data_blocks_in_file, &temp) == false) {
    					printf("\nError in fs_remove: could not extract from dyn array");
	    				dyn_array_destroy(data_blocks_in_file);
	    				return -1;
	    			}
    				back_store_release(fs->bs, temp);
    			}

    			dyn_array_destroy(data_blocks_in_file);

    			// remove the inode from the parent directory
				inode_t parent_inode;
				read_inode(fs, &parent_inode, file_status.parent);

				dir_block_t parent_dir;
				back_store_read(fs->bs, parent_inode.data_ptrs[0], &parent_dir);

				inode_t found_inode;
				read_inode(fs, &found_inode, file_status.inode);

				bool is_found = false;
				for(int i=0; i<DIR_REC_MAX; ++i) {
					if (strcmp(parent_dir.entries[i].fname, found_inode.fname) == 0) {
						parent_dir.entries[i].fname[0] = '\0';
						is_found = true;
						break;
					}
				}
				if (is_found == false) {
					printf("\nError with fs_remove: The directory to be deleted was not found in the parent dir");
					return -1;
				}

				// change the filename to NULL or 0
    			if (clear_inode(fs, file_status.inode) == false) {
					printf("\nError in fs_remove: clear_inode function failec");
					return -1;
				}
				// Write parent dir back to BS from memory
				if (full_write(fs, (void *)&parent_dir, parent_inode.data_ptrs[0]) == true) {
    				// return 0
    				return 0;
    			} else {
    				printf("\nError with fs_remove: The parent_dir could not be written to the bs");
    				return -1;
    			}
    		} else if (file_status.type == FS_DIRECTORY) {
    			// check to see if directory is empty or not 
    			dir_block_t temp_dir;

    			back_store_read(fs->bs, file_status.block, &temp_dir);

    			bool is_empty = dir_is_empty(fs, &temp_dir);
    			// if empty 
    			if (is_empty == true) {
    				// clear the block holding the dir_block 
    				back_store_release(fs->bs, file_status.block);
    				// remove from parent directory 
    				inode_t parent_inode;
    				read_inode(fs, &parent_inode, file_status.parent);

    				dir_block_t parent_dir;
    				back_store_read(fs->bs, parent_inode.data_ptrs[0], &parent_dir);

    				inode_t found_inode;
    				read_inode(fs, &found_inode, file_status.inode);

    				bool is_found = false;
    				for(int i=0; i<DIR_REC_MAX; ++i) {
    					if (strcmp(parent_dir.entries[i].fname, found_inode.fname) == 0) {
    						parent_dir.entries[i].fname[0] = '\0';
    						is_found = true;
    						break;
    					}
    				}
    				if (is_found == false) {
    					printf("\nError with fs_remove: The directory to be deleted was not found in the parent dir");
    					return -1;
    				}

    				// clear inode and write to BS
    				if (clear_inode(fs, file_status.inode) == false) {
    					printf("\nError in fs_remove: clear_inode function failec");
    					return -1;
    				}
    				// Write parent dir back to BS from memory
    				if (full_write(fs, (void *)&parent_dir, parent_inode.data_ptrs[0]) == true) {
	    				// return 0
	    				return 0;
	    			} else {
	    				printf("\nError with fs_remove: The parent_dir could not be written to the bs");
	    				return -1;
	    			}
    			} else { // else 
    				printf("\nError with fs_remove: The directory requested to be remove is not empty");
    				for (int i = 0; i < DIR_REC_MAX; ++i) {
				    }
    				return -1;
    			}
    		} else {
    			printf("\nError with fs_remove: the inode found is a directory, not a file");
    			return -1;
    		}
    	} else {
    		printf("\nError with fs_remove: the file was not found from locate_file, but the function returned successfully");
    		return -1;
    	}
    } else {
    	printf("\nError with fs_remove: locate_file function ran into an error");
    	return -1;
    }

    return -1;
}

///
/// Moves the R/W position of the given descriptor to the given location
///   Files cannot be seeked past EOF or before BOF (beginning of file)
///   Seeking past EOF will seek to EOF, seeking before BOF will seek to BOF
/// \param fs The S16FS containing the file
/// \param fd The descriptor to seek
/// \param offset Desired offset relative to whence
/// \param whence Position from which offset is applied
/// \return offset from BOF, < 0 on error
///
off_t fs_seek(S16FS_t *fs, int fd, off_t offset, seek_t whence) {
    if (!fs || !fd || fd < 0 || fd > 255) return -1;

    inode_t file;
    if (read_inode(fs, &file, fs->fd_table.fd_inode[fd]) != true) {
        printf("\nfs_seek error: could not read inode to check size");
    }

    if(!offset || offset < 0 || (uint32_t)offset > file.mdata.size ) return -1;

    // FS_SEEK_SET, FS_SEEK_CUR, FS_SEEK_END
    if (whence == FS_SEEK_SET) {
        fs->fd_table.fd_pos[fd] = offset;
        return offset;
    } else if (whence == FS_SEEK_CUR) {
        if ((fs->fd_table.fd_pos[fd]) + (uint32_t)offset > file.mdata.size) {
            fs->fd_table.fd_pos[fd] = file.mdata.size;
            return file.mdata.size;
        } else {
            fs->fd_table.fd_pos[fd] = (fs->fd_table.fd_pos[fd]) + offset;
            return (fs->fd_table.fd_pos[fd]) + offset;
        }
    } else if (whence == FS_SEEK_END) {
        fs->fd_table.fd_pos[fd] = file.mdata.size - offset;
        return file.mdata.size - offset;
    } else {
        printf("\nInvalid whence variable");
        return -1;
    }

    return -1;
}

///
/// Reads data from the file linked to the given descriptor
///   Reading past EOF returns data up to EOF
///   R/W position in incremented by the number of bytes read
/// \param fs The S16FS containing the file
/// \param fd The file to read from
/// \param dst The buffer to write to
/// \param nbyte The number of bytes to read
/// \return number of bytes read (< nbyte IFF read passes EOF), < 0 on error
///
ssize_t fs_read(S16FS_t *fs, int fd, void *dst, size_t nbyte) {
    if (fs && dst && fd_valid(fs, fd)) {
        if (nbyte != 0) {

            inode_t file_inode;
            
            size_t *current_position = &fs->fd_table.fd_pos[fd];

            if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd])) {
                if ((current_position + nbyte) > (size_t *)file_inode.mdata.size) {
                    if (nbyte) {
                        ssize_t read = read_file(fs, &file_inode, fs->fd_table.fd_pos[fd], dst, nbyte);
                        if (read > 0) {
                            fs->fd_table.fd_pos[fd] += read;
                        }
                        return read;
                    }
                    return 0;
                }
            }
        } else {
            return 0;
        }
    }
    return -1;
}

///
/// Populates a dyn_array with information about the files in a directory
///   Array contains up to 15 file_record_t structures
/// \param fs The S16FS containing the file
/// \param path Absolute path to the directory to inspect
/// \return dyn_array of file records, NULL on error
///
dyn_array_t *fs_get_dir(S16FS_t *fs, const char *path) {
    if (!fs || !path || strcmp(path, "") == 0 || strcmp(path, "\n") == 0) return NULL;



    return NULL;
}

///
/// !!! Graduate Level/Undergrad Bonus !!!
/// !!! Activate tests from the cmake !!!
///
/// Moves the file from one location to the other
///   Moving files does not affect open descriptors
/// \param fs The S16FS containing the file
/// \param src Absolute path of the file to move
/// \param dst Absolute path to move the file to
/// \return 0 on success, < 0 on error
///
int fs_move(S16FS_t *fs, const char *src, const char *dst);