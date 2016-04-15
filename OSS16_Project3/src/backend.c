#include "backend.h"

#include <string.h>
#include <time.h>

// Ok, less worse than a RMW, but it isn't very space efficient
// One neat idea would be to make a shared buffer a global so both can use it
// Since we're soooooo not thread safe anyway, it could save some space... kinda?
// Maybe not since it would be a constant 1k being taken up as opposed to the occasional 1k
// Hmm.
bool partial_read(const S16FS_t *fs, void *data, const block_ptr_t block, const unsigned offset, const unsigned bytes) {
    if (fs && data && BLOCK_PTR_VALID(block) && offset < BLOCK_SIZE && bytes) {
        data_block_t buffer;
        if (back_store_read(fs->bs, block, &buffer)) {
            memcpy(data, &buffer + offset, bytes);
            return true;
        }
    }
    return false;
}

// THE DREADED READ MODIFY WRITE. Avoid it at all costs. Generally. Unless it's just the one thing. That's cool.
bool partial_write(S16FS_t *fs, const void *data, const block_ptr_t block, const unsigned offset,
                   const unsigned bytes) {
    if (fs && data && BLOCK_PTR_VALID(block) && offset < BLOCK_SIZE && bytes) {
        // if (bytes == 0) return true; // just in case my logic gets weird somewhere
        // but that won't "allow" ofset = 1024 and bytes = 0
        // Scratch that, return false. If it actually happens, it should be reported
        data_block_t buffer;
        if (back_store_read(fs->bs, block, &buffer)) {
            printf("\n%zu\n%zu\n%zu\n", offset, bytes, sizeof(data));
            memcpy(&buffer[offset], data, bytes);
            printf("\nPARTIAL WRITE\n");
            return back_store_write(fs->bs, block, &buffer);
        }
    }
    return false;
}

bool read_inode(const S16FS_t *fs, void *data, const inode_ptr_t inode_number) {
    if (fs && data) {
        inode_t buffer[INODES_PER_BOCK];
        if (back_store_read(fs->bs, INODE_TO_BLOCK(inode_number), buffer)) {
            memcpy(data, &buffer[INODE_INNER_IDX(inode_number)], sizeof(inode_t));
            return true;
        }
    }
    return false;
}

bool write_inode(S16FS_t *fs, const void *data, const inode_ptr_t inode_number) {
    if (fs && data) {  // checking if the inode number is valid is a tautology :/
        inode_t buffer[INODES_PER_BOCK];
        if (back_store_read(fs->bs, INODE_TO_BLOCK(inode_number), buffer)) {
            memcpy(&buffer[INODE_INNER_IDX(inode_number)], data, sizeof(inode_t));
            return back_store_write(fs->bs, INODE_TO_BLOCK(inode_number), buffer);
        }
    }
    return false;
}

bool clear_inode(S16FS_t *fs, const inode_ptr_t inode_number) {
    // Just going to blank the first fname character.
    // Allows for easier post-mortem debugging than completely blanking it
    if (fs) {
        inode_t buffer[INODES_PER_BOCK];
        if (back_store_read(fs->bs, INODE_TO_BLOCK(inode_number), buffer)) {
            buffer[INODE_INNER_IDX(inode_number)].fname[0] = '\0';
            return back_store_write(fs->bs, INODE_TO_BLOCK(inode_number), buffer);
        }
    }
    return false;
}

// might as well make versions that do whole blocks. Better encapsulation?
// All calls are verified a bit more before happening, which is good.
bool full_read(const S16FS_t *fs, void *data, const block_ptr_t block) {
    if (fs && data) {  // you can read from the inode table...
        return back_store_read(fs->bs, block, data);
    }
    return false;
}

bool full_write(S16FS_t *fs, const void *data, const block_ptr_t block) {
    if (fs && data && block >= DATA_BLOCK_OFFSET) {  // but you can't write to it. Not in bulk.
                                                     // there is NO reason to do a bulk write to the inode table
        return back_store_write(fs->bs, block, data);
    }
    return false;
}


S16FS_t *ready_file(const char *path, const bool format) {
    S16FS_t *fs = (S16FS_t *) malloc(sizeof(S16FS_t));
    if (fs) {
        if (format) {
            // get inode table
            // format root
            // That's it?

            // oh, also, ya know, make the back store object. oops.
            fs->bs = back_store_create(path);
            if (fs->bs) {
                bool valid = true;
                // + 1 to snag the root dir block because lazy
                for (int i = INODE_BLOCK_OFFSET; i < (DATA_BLOCK_OFFSET + 1) && valid; ++i) {
                    valid &= back_store_request(fs->bs, i);
                }
                // inode table is already blanked because back_store blanks all data (woo)
                if (valid) {
                    // I'm actually not sure how to do this
                    // It's going to look like a mess
                    uint32_t right_now = time(NULL);
                    inode_t root_inode = {"/",
                                          {0, 0777, right_now, right_now, right_now, 0, FS_DIRECTORY, {0}},
                                          {DATA_BLOCK_OFFSET, 0, 0, 0, 0, 0, 0, 0}};
                    // fname technically invalid, but it's root so deal
                    // mdata actually might not be used in a dir record. Idk.
                    // block pointer set, rest are invalid
                    // break point HERE to make sure that constructed right
                    valid &= write_inode(fs, &root_inode, 0);
                }
                if (!valid) {
                    // weeeeeeeh
                    back_store_close(fs->bs);
                    fs->bs = NULL;  // just set it and trigger the free elsewhere
                }
            }
        } else {
            fs->bs = back_store_open(path);
            // ... that's it?
        }
        if (fs->bs) {
            fs->fd_table.fd_status = bitmap_create(DESCRIPTOR_MAX);
            // Eh, won't bother blanking out tables, since that's the point of the bitmap
            if (fs->fd_table.fd_status) {
                return fs;
            }
        }
        free(fs);
    }
    return NULL;
}

/*
hunts down the requested file, if it exists, filling out all sorts of little bits of data

typedef struct {
    bool success; - Did the operation complete? (generally just parameter issues)
    bool found; - Did we find the file? Does it exist?
    bool valid; - N/A
    inode_ptr_t inode; - IF FOUND: inode of file
    inode_ptr_t parent; - IF FOUND: inode of parent directory of file
    block_ptr_t block; - IF FOUND AND TYPE DIRECTORY: Dir block for directory
    file_t type; - IF FOUND: Type of file
    uint64_t total; - N/A
    void *data; - Pointer to last token parsed (in given path string)
                    Either the filename, or the token we died on
} result_t;

*/

void locate_file(const S16FS_t *const fs, const char *abs_path, result_t *res) {
    if (res) {
        memset(res, 0x00, sizeof(result_t));  // IMMEDIATELY blank it
        if (fs && abs_path) {
            // need to get the token processing loop started (oh boy!)
            const size_t path_len = strnlen(abs_path, FS_PATH_MAX);
            if (path_len != 0 && abs_path[0] == '/' && path_len < FS_PATH_MAX) {
                // ok, path is something we should at least bother trying to look at
                char *path_copy = strndup(abs_path, path_len);
                if (path_copy) {
                    result_t scan_results = {true, true, false, 0, 0, 0, 0, 0, NULL};  // hey, cool, I found root!
                    const char *delims    = "/";
                    res->success          = true;
                    res->found            = true;  // I'm going to assume it all works out, don't go making me a liar
                    res->inode            = 0;
                    res->block            = ROOT_DIR_BLOCK;
                    res->type             = FS_DIRECTORY;
                    res->data             = (void *) abs_path;
                    char *token           = strtok(path_copy, delims);
                    // Hardcoding results for root in case there aren't any tokens (path was "/")

                    while (token) {
                        // update the dir pointer
                        res->data = (void *) (abs_path + (token - path_copy));

                        // Cool. Does the next token exist in the current directory?
                        scan_directory(fs, token, scan_results.inode, &scan_results);

                        if (scan_results.success && scan_results.found) {
                            // Good. It existed. Cycle.
                            res->parent = scan_results.parent;
                            res->inode  = scan_results.inode;
                            token       = strtok(NULL, delims);
                            continue;
                        }
                        // welp. Something's broken. File not found.
                        res->found = false;
                        break;
                    }
                    if (res->found) {
                        inode_t found_file;
                        if (read_inode(fs, &found_file, res->inode)) {
                            res->type = found_file.mdata.type;
                            if (res->type == FS_DIRECTORY) {
                                res->block = found_file.data_ptrs[0];
                            }
                            free(path_copy);
                            return;
                        }
                        // back_store ate it I guess? That's bad.
                        // What do we do now? (die.)
                        memset(res, 0x00, sizeof(result_t));  // all that work for nothing
                    }
                    free(path_copy);
                }
            }
        }
    }
}

/*
Flips through the specified directory, finding the specified file (hopefully)

typedef struct {
    bool success; - Did the operation complete? (generally just parameter issues) Was it a dir?
    bool found; - IF VALID: Did we find the file? Does it exist?
    bool valid; - Was the filename valid?
    inode_ptr_t inode; - IF FOUND: inode of file
    inode_ptr_t parent; - IF SUCCESS: Literally the inode number you fed us
    block_ptr_t block; - IF SUCCESS: Data block of directory.
                            Shouldn't never need it, but we know it, so we'll share
    file_t type; - IF FOUND: type of the file found
    uint64_t total; - IF SUCCESS: Number of files in the given directory
    void *data; - N/A
} result_t;
*/
void scan_directory(const S16FS_t *const fs, const char *fname, const inode_ptr_t inode, result_t *res) {
    if (res) {
        memset(res, 0x00, sizeof(result_t));
        if (fs && fname) {
            // inode number is always valid - tbh, that may mask errors and could be considered bad
            inode_t dir_inode;
            dir_block_t dir_data;
            if (read_inode(fs, &dir_inode, inode) && INODE_IS_TYPE(&dir_inode, FS_DIRECTORY)
                && full_read(fs, &dir_data, dir_inode.data_ptrs[0])) {
                res->success = true;
                res->block   = dir_inode.data_ptrs[0];
                res->total   = dir_data.mdata.size;
                res->parent  = inode;
                // let's validate the fname
                const size_t fname_len = strnlen(fname, FS_FNAME_MAX);
                if (fname_len != 0 && fname_len < FS_FNAME_MAX) {
                    // Alrighty, we got the inode and block read in.
                    // fname is vaguely validated
                    res->valid = true;
                    for (unsigned i = 0; i < DIR_REC_MAX; ++i) {
                        if (strncmp(fname, dir_data.entries[i].fname, FS_FNAME_MAX) == 0) {
                            // found it!
                            res->found = true;
                            res->inode = dir_data.entries[i].inode;
                            return;
                        }
                    }
                }
            }
        }
    }
}

// Just what it sounds like. 0 on error
inode_ptr_t find_free_inode(const S16FS_t *const fs) {
    if (fs) {
        inode_t inode_block[INODES_PER_BOCK];
        inode_ptr_t free_inode = 0;
        for (unsigned blk = INODE_BLOCK_OFFSET; blk < DATA_BLOCK_OFFSET; ++blk) {
            if (full_read(fs, &inode_block, blk)) {
                for (unsigned i = 0; i < INODES_PER_BOCK; ++i, ++free_inode) {
                    if (inode_block[i].fname[0] == '\0') {
                        return free_inode;
                        // potentially a conversion warning because integer truncation/depromotion
                    }
                }
            } else {
                return 0;  // :/
            }
        }
    }
    return 0;
}

bool dir_is_empty(const S16FS_t *const fs, dir_block_t *dir) {
    if (!fs || !dir) return false;

    for (int i = 0; i < DIR_REC_MAX; ++i) {
        if (dir->entries[i].fname[0] != '\0') {
            return false;
        }
    }
    return true;
}

dyn_array_t* build_array_of_file_data_ptrs(const S16FS_t *const fs, inode_ptr_t file) {
    if (!fs || !file) {
        printf("\nerror: bad params");
        return NULL;
    }

    inode_t inode;
    if (read_inode(fs, &inode, file) != true) {
        printf("\nError with building data ptr array: could not read inode");
        return NULL;
    }

    dyn_array_t *blocks_array = dyn_array_create(0,sizeof(block_ptr_t),NULL);

    for (int i=0; i<6; ++i) { // this gets the 6 direct pointers, then move onto indirect and double indirect
        if (inode.data_ptrs[i] != 0) {
            if (dyn_array_push_back(blocks_array, (void *)&(inode.data_ptrs[i])) != true) {
                printf("\nError with building data ptr array: could not add to array");
                dyn_array_destroy(blocks_array);
                return NULL;
            }
        }
    }

    // add indirect
    if (inode.data_ptrs[6] != 0) {
        indir_block_t first_indirect_block;
        if (full_read(fs, &first_indirect_block, inode.data_ptrs[6]) != true) {
            printf("\nError with building data ptr array: could not read first indirect pointer block");
            dyn_array_destroy(blocks_array);
            return NULL;
        }

        for (size_t i=0; i<INDIRECT_TOTAL; ++i) {
            if (first_indirect_block.block_ptrs[i] != 0) {
                if (dyn_array_push_back(blocks_array, (void *)&(first_indirect_block.block_ptrs[i])) != true) {
                    printf("\nError with building data ptr array: could not add to array");
                    dyn_array_destroy(blocks_array);
                    return NULL;
                }
            }
        }
    }
    // add double indirect
    if (inode.data_ptrs[7] != 0) {
        indir_block_t double_indirect;
        if (full_read(fs, &double_indirect, inode.data_ptrs[7]) != true) {
            printf("\nError with building data ptr array: could not read first indirect pointer block");
            dyn_array_destroy(blocks_array);
            return NULL;
        }

        for (size_t i=0; i<INDIRECT_TOTAL; ++i) {
            if (double_indirect.block_ptrs[i] != 0) {
                indir_block_t indirect_block;
                if (full_read(fs, &indirect_block, double_indirect.block_ptrs[i]) != true) {
                    printf("\nError with building data ptr array: could not read first indirect pointer block");
                    dyn_array_destroy(blocks_array);
                    return NULL;
                }

                for (size_t j=0; j<INDIRECT_TOTAL; ++j) {
                    if (indirect_block.block_ptrs[j] != 0) {
                        if (dyn_array_push_back(blocks_array, (void *)&(indirect_block.block_ptrs[j])) != true) {
                            printf("\nError with building data ptr array: could not add to array");
                            dyn_array_destroy(blocks_array);
                            return NULL;
                        }
                    }
                }
            }
        }
    }

    return blocks_array;
}

dyn_array_t* build_data_ptrs_array(S16FS_t *fs, size_t num_blocks_to_write, const void *src, int fd, size_t nbyte, size_t *bytes_written, size_t *blocks_written) {
    // build dyn array of data blocks written to 
    inode_t file_inode;
    if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd]) == false) {
        printf("\nerror: Could not read inode of file from fd");
        return NULL;
    }

    dyn_array_t *data_blocks_written_to = dyn_array_create(0,sizeof(block_ptr_t),NULL);
    size_t fd_pos = fs->fd_table.fd_pos[fd]; // current 
    size_t fd_pos_block = fd_pos/BLOCK_SIZE;
    bool start_of_block = fd_pos%BLOCK_SIZE == 0;

    while (*blocks_written < num_blocks_to_write) {
        if (fd_pos_block >= 262662) {
            printf("\nerror: Trying to extend file past the maximum file size");
            dyn_array_destroy(data_blocks_written_to);
            return NULL;
        }

        if (*blocks_written == 0 && start_of_block == false) {

            block_ptr_t new_block = find_block(fs, fd_pos_block, fd);

            if (new_block == 0) {
                new_block = (block_ptr_t)back_store_allocate(fs->bs);
                //printf("\nUH OH: ---> %zu\n", new_block);
                if (new_block == 0) {
                    printf("\nerror: NO ROOM to allocate new block");
                    return data_blocks_written_to;
                }
            }
            // calculate offset
            size_t offset = fd_pos - (fd_pos_block*BLOCK_SIZE);
            size_t num_bytes = BLOCK_SIZE-offset;

            //printf("\noffset: %zu\nnum_bytes: %zu\n", offset, num_bytes);
            // write to block in bs (partial write???)
            if (partial_write(fs, src, new_block, offset, num_bytes) != true) {
                printf("\nerror: can't partial write");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }
            // add to dyn_array
            if (dyn_array_push_back(data_blocks_written_to, &new_block) != true) {
                printf("\nerror: cannot push to back of array");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }

            *bytes_written += num_bytes;
            // increment the void pointer only by however many bytes are used... 
            src = INCREMENT_VOID(src, num_bytes);
        } else if (*blocks_written == 0 && start_of_block == true && nbyte < BLOCK_SIZE) {
            // read block # to local variable
            block_ptr_t new_block = find_block(fs, fd_pos_block, fd);

            if (new_block == 0) {
                new_block = (block_ptr_t)back_store_allocate(fs->bs);
                //printf("\nUH OH: ---> %zu\n", new_block);
                if (new_block == 0) {
                    printf("\nerror: NO ROOM to allocate new block");
                    return data_blocks_written_to;
                }
            }
            // calculate offset
            size_t offset = 0;
            size_t num_bytes = nbyte;
            // write to block in bs (partial write???)
            if (partial_write(fs, src, new_block, offset, num_bytes) != true) {
                printf("\nerror: cannot partial writeee");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }
            // add to dyn_array
            if (dyn_array_push_back(data_blocks_written_to, &new_block) != true) {
                printf("\nerror: cannot push to backkkk of array");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }

            *bytes_written += num_bytes;
            // increment the void pointer only by however many bytes are used... 
            src = INCREMENT_VOID(src, num_bytes);
        } else if (*blocks_written == 0 && start_of_block == true && nbyte >= BLOCK_SIZE) {
            
            block_ptr_t new_block = find_block(fs, fd_pos_block, fd);

            if (new_block == 0) {
                new_block = (block_ptr_t)back_store_allocate(fs->bs);
                //printf("\nUH OH: ---> %zu\n", new_block);
                if (new_block == 0) {
                    printf("\nerror: NO ROOM to allocate new block");
                    return data_blocks_written_to;
                }
            }

            // write to block in bs (partial write???)
            if (full_write(fs, src, new_block) != true) {
                printf("\nerror: cannot full write 2");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }
            // add to dyn_array
            if (dyn_array_push_back(data_blocks_written_to, &new_block) != true) {
                printf("\nerror: cannot push to back 2");
                dyn_array_destroy(data_blocks_written_to);
                return NULL;
            }

            *bytes_written += BLOCK_SIZE;
            // increment the void pointer only by however many bytes are used... 
            src = INCREMENT_VOID(src, BLOCK_SIZE);
        } else {
            if (*blocks_written == num_blocks_to_write-1 && (fd_pos + nbyte)%BLOCK_SIZE != 0) { // if the last block of data doesn't fill up the entire block
               
                block_ptr_t new_block = find_block(fs, fd_pos_block, fd);

                if (new_block == 0) {
                    new_block = (block_ptr_t)back_store_allocate(fs->bs);
                    //printf("\nUH OH: ---> %zu\n", new_block);
                    if (new_block == 0) {
                        printf("\nerror: NO ROOM to allocate new block");
                        return data_blocks_written_to;
                    }
                }

                // calculate offset
                size_t offset = 0;
                size_t num_bytes = (fd_pos + nbyte)-(BLOCK_SIZE*fd_pos_block);
                // write to block in bs (partial write???)
                if (partial_write(fs, src, new_block, offset, num_bytes) != true) {
                    dyn_array_destroy(data_blocks_written_to);
                    printf("\nerror: partial write failed");
                    return NULL;
                }
                // add to dyn_array
                if (dyn_array_push_back(data_blocks_written_to, &new_block) != true) {
                    dyn_array_destroy(data_blocks_written_to);
                    printf("\nerror: push to back of array failed");
                    return NULL;
                }

                *bytes_written += num_bytes;
                // increment the void pointer only by however many bytes are used... 
                src = INCREMENT_VOID(src, num_bytes);
            } else {
                // find new block and (allocate???) ir wait until adding to data_ptrs
                
                block_ptr_t new_block = find_block(fs, fd_pos_block, fd);

                if (new_block == 0) {
                    new_block = (block_ptr_t)back_store_allocate(fs->bs);
                    //printf("\nUH OH: ---> %zu\n", new_block);
                    if (new_block == 0) {
                        printf("\nError in fs_Write: NO ROOM to allocate new block");
                        return data_blocks_written_to;
                    }
                }

                //printf("\nBLOCK: %zu, fd_block: %zu  num to write: %zu", new_block, fd_pos_block, num_blocks_to_write);

                // write to block (full write???)
                if (full_write(fs, src, new_block) != true) {
                    printf("\nerror: error with full_Write");
                    dyn_array_destroy(data_blocks_written_to);
                    return NULL;
                }

                // add to dyn_array
                if (dyn_array_push_back(data_blocks_written_to, &new_block) != true) {
                    printf("\nerror: error with push to dyn array back");
                    dyn_array_destroy(data_blocks_written_to);
                    return NULL;
                }

                *bytes_written += BLOCK_SIZE;
                // Increment void pointer for src
                src = INCREMENT_VOID(src, BLOCK_SIZE);
            }
        }

        *blocks_written += 1;
        fd_pos_block++;
    }
    //printf("\nEND OF FUNCTION ---> BYTES: %u\n", *blocks_written);
    //printf("\nDONE");
    return data_blocks_written_to;
}

block_ptr_t find_block(S16FS_t *fs, size_t fd_pos_block, int fd) {
    inode_t file_inode;
    if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd]) == false) {
        printf("\nerror: Could not read inode of file from fd");
        return 0;
    }

    //printf("\nFD_POS: %zu", fd_pos_block);
    block_ptr_t new_block = 0;

    if (fd_pos_block <= 5) { // direct pointers
        new_block = file_inode.data_ptrs[fd_pos_block];
    } else if (fd_pos_block >= 6 && fd_pos_block < 518) {
        if (file_inode.data_ptrs[6] == 0) {
            new_block = (block_ptr_t)back_store_allocate(fs->bs);
            //printf("\nINDIR BLOCK: ---> %zu\n", new_block);
            if (new_block == 0) {
                printf("\nfs_write error: Nope room to add the indirect pointer block");
                return 0;
            }

            indir_block_t indirect_block;
            memset(&indirect_block, 0, sizeof(indir_block_t));

            if (full_write(fs, (void *)&indirect_block, new_block) != true) {
                printf("\nfs_write error: could not write indirect block to bs");
                return 0;
            }

            file_inode.data_ptrs[6] = new_block;

            if (write_inode(fs, (void *)&file_inode, fs->fd_table.fd_inode[fd]) != true) {
                printf("\nfs_write error: Could not write inode to BS");
                return 0;
            }

            new_block = 0;
        } else {
            indir_block_t temp;
            if (full_read(fs, &temp, file_inode.data_ptrs[6]) != true) {
                printf("\nerror: could not read indirect pointer");
                return 0;
            }
            new_block = temp.block_ptrs[fd_pos_block-6];
        }
    } else if (fd_pos_block >= 518) {
        if (file_inode.data_ptrs[7] == 0) {
            block_ptr_t new_dbl_block = (block_ptr_t)back_store_allocate(fs->bs);
            if (new_dbl_block == 0) {
                printf("\nfs_write error: Noo room to add the double indirect pointer block");
                return 0;
            }
            //printf("\nDBL INDIR BLOCK: ---> %zu\n", new_dbl_block);

            file_inode.data_ptrs[7] = new_dbl_block;

            indir_block_t double_indirect_block;
            memset(&double_indirect_block, 0, sizeof(indir_block_t));

            new_block = (block_ptr_t)back_store_allocate(fs->bs);
            //printf("\nINDIR IN DBL BLOCK: ---> %zu\n", new_block);
            if (new_block == 0) {
                printf("\nfs_write error: Nooo room to add the indirect pointer block");
                return 0;
            }

            double_indirect_block.block_ptrs[0] = new_block;

            if (full_write(fs, (void *)&double_indirect_block, new_dbl_block) != true) {
                printf("\nfs_write error: could not write indirect block to bs");
                return 0;
            }

            indir_block_t indirect_block;
            memset(&indirect_block, 0, sizeof(indir_block_t));

            if (full_write(fs, (void *)&indirect_block, new_block) != true) {
                printf("\nfs_write error: could not write indirect block to bs");
                return 0;
            }

            if (write_inode(fs, (void *)&file_inode, fs->fd_table.fd_inode[fd]) != true) {
                printf("\nfs_write error: Could not write inode to BS");
                return 0;
            }

            new_block = 0;
        } else {    
            size_t double_indirect_index = (fd_pos_block-518)/(INDIRECT_TOTAL);
            size_t indirect_index = (fd_pos_block-518) - (double_indirect_index*INDIRECT_TOTAL);

            indir_block_t double_indirect_block;
            if (full_read(fs, &double_indirect_block, file_inode.data_ptrs[7]) != true) {
                printf("\nerror: could not read double indirect pointer");
                return 0;
            }

            //printf("\nDBL INDIR INDEX: %zu\nPTR: %zu", double_indirect_index, double_indirect_block.block_ptrs[double_indirect_index]);
            if (double_indirect_block.block_ptrs[double_indirect_index] == 0) {
                new_block = (block_ptr_t)back_store_allocate(fs->bs);
                //printf("\nINDIR IN DBL INDIR BLOCK: %zu", new_block);
                if (new_block == 0) {
                    printf("\nfs_write error: Nope room to add the indirect pointer block");
                    return 0;
                }

                indir_block_t indirect_block;
                memset(&indirect_block, 0, sizeof(indir_block_t));

                if (full_write(fs, (void *)&indirect_block, new_block) != true) {
                    printf("\nfs_write error: could not write indirect block to bs");
                    return 0;
                }

                double_indirect_block.block_ptrs[double_indirect_index] = new_block;

                if (full_write(fs, (void *)&double_indirect_block, file_inode.data_ptrs[7]) != true) {
                    printf("\nfs_writeee error: could not write indirect block to bs");
                    return 0;
                }

                if (write_inode(fs, (void *)&file_inode, fs->fd_table.fd_inode[fd]) != true) {
                    printf("\nfs_write error: Could not write inode to BS");
                    return 0;
                }

                new_block = 0;
            } else {
                indir_block_t indirect_block;
                if (full_read(fs, &indirect_block, double_indirect_block.block_ptrs[double_indirect_index]) != true) {
                    printf("\nerror: could not read indirect pointer");
                    return 0;
                }

                new_block = indirect_block.block_ptrs[indirect_index];
            }
        }
    } 
    return new_block;
}