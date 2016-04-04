#include "S16FS.h"

// metadata, inode, S16FS_t ---> padding at end???

// directory, directory block, file descriptor tables ---> From design 

S16FS_t *fs_format(const char *path) {
	if (strcmp(path, "") == 0 || strcmp(path, "\n") == 0 || !path) return NULL;

	return NULL;
}

S16FS_t *fs_mount(const char *path) {
	if (strcmp(path, "") == 0 || strcmp(path, "\n") == 0 || !path) return NULL;

	return NULL;
}

int fs_unmount(S16FS_t *fs) {
	if (!fs) return -1;

	return -1;
}

int fs_create(S16FS_t *fs, const char *path, file_t type) {
	if (!fs || strcmp(path, "") == 0 || strcmp(path, "\n") == 0 || !path || !type) return -1;

	return -1;
}
