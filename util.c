/*util.c - Utility functions for ls*/

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ls.h"

/*Compare by modification (m)time*/
int compare_timem(const void *a, const void *b) {
	const struct file_entry *fa = (const struct file_entry *)a;
	const struct file_entry *fb = (const struct file_entry *)b;
	if (fa->sb.st_mtime > fb->sb.st_mtime) {
		return -1;
	} 
	else if (fa->sb.st_mtime < fb->sb.st_mtime) {
		return 1;
	}
	return strcmp(fa->name, fb->name);
}
/*Compare by access (a)time*/
int compare_timea(const void *a, const void *b) {
	const struct file_entry *fa = (const struct file_entry *)a;
	const struct file_entry *fb = (const struct file_entry *)b;
	if (fa->sb.st_atime > fb->sb.st_atime) {
		return -1;
	} 
	else if (fa->sb.st_atime < fb->sb.st_atime) {
		return 1;
	}
	return strcmp(fa->name, fb->name);
}
/*Compare by status change (c)time*/
int compare_timec(const void *a, const void *b) {
	const struct file_entry *fa = (const struct file_entry *)a;
	const struct file_entry *fb = (const struct file_entry *)b;
	if (fa->sb.st_ctime > fb->sb.st_ctime) {
		return -1;
	} 
	else if (fa->sb.st_ctime < fb->sb.st_ctime) {
		return 1;
	}
	return strcmp(fa->name, fb->name);
}
/*compare by size (largest first)*/
int compare_size(const void *a, const void *b) {
	const struct file_entry *fa = (const struct file_entry *)a;
	const struct file_entry *fb = (const struct file_entry *)b;

	if (fa->sb.st_size > fb->sb.st_size) {
		return -1;
	} else if (fa->sb.st_size < fb->sb.st_size) {
		return 1;
	}
	return strcmp(fa->name, fb->name);
}

/*reverse array of entries*/
void reverse_entries(struct file_entry *entries, int count) {
	int i, j;
	struct file_entry temp;

	for (i = 0, j = count - 1; i < j; i++, j--) {
		temp = entries[i];
		entries[i] = entries[j];
		entries[j] = temp;
	}
}

bool is_directory(const char *path) {
	struct stat sb;
	if (stat(path, &sb) < 0) {
		return false;
	}
	return S_ISDIR(sb.st_mode);
}

/*Build full path from directory and filename.
 caller frees whats returned*/
char * build_path(const char *dir, const char *file){
	char *path;
	size_t dir_len;
	size_t file_len;
	size_t total_len;

	dir_len = strlen(dir);
	file_len = strlen(file);

	total_len = dir_len + 1 + file_len + 1;
	if (total_len > PATH_MAX) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	if ((path = malloc(total_len)) == NULL) {
		return NULL;
	}
	/*put together*/
	(void)strcpy(path, dir);

	/*add slash if dir dont end with one*/
	if (dir_len > 0 && dir[dir_len - 1] != '/') {
		(void)strcat(path, "/");
	}
	(void)strcat(path, file);
	return path;
}

/*helper to comp two file entries by name for alphabetic sort*/
int compare_names(const void *a, const void *b) {
	const struct file_entry *fa;
	const struct file_entry *fb;
	fa = (const struct file_entry *)a;
	fb = (const struct file_entry *)b;
	return strcmp(fa->name, fb->name);
}

uint64_t get_display_block_size(const struct stat *sb, const struct options *opts) {
    uint64_t blocks = sb->st_blocks;  //st_blocks is in 512-byte units
    //conv to bytes
    uint64_t bytes = blocks * 512;
    //-h or -k
    if (opts->human_readable) {
        return bytes;
    } 
	else if (opts->kilobytes) {
		/*round to kb*/
        return (bytes + 1023) / 1024; 
    }
	else { 
		/*default 512 blocks*/
        return blocks;
    }
}
const char *format_size(uint64_t bytes, char *buf, size_t buflen) {
    const char *units[] = {"B", "K", "M", "G", "T", "P"};
    int i = 0;
    double size = (double)bytes;
    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }
    snprintf(buf, buflen, "%.1f%s", size, units[i]);
    return buf;
}
/*Sort entries with opts flags*/
void sort_entries(struct file_entry *entries, int count, const struct options *opts){
	/*no sort if -f*/
	if (opts->unsorted) {
		return;
	}
	/*choose sort function*/
	if (opts->sort_time) {
		if(opts->use_atime){
			qsort(entries, count, sizeof(struct file_entry), compare_timea);
		}
		else if(opts->use_ctime){
			qsort(entries, count, sizeof(struct file_entry), compare_timec);
		}
		else {
			qsort(entries, count, sizeof(struct file_entry), compare_timem);
		}
	} 
	else if (opts->sort_size) {
		qsort(entries, count, sizeof(struct file_entry), compare_size);
	}
	else {
		qsort(entries, count, sizeof(struct file_entry), compare_names);
	}

	/*reverse if -r*/
	if (opts->reverse) {
		reverse_entries(entries, count);
	}
}

