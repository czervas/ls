/*ls.h - header file for ls*/

#ifndef _LS_H_
#define _LS_H_

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <limits.h>
#include <stdbool.h>

/*command line options*/
struct options {
	bool show_all;       /* -a all . files including . and .. */
	bool show_almost_all;  /*-A all . files except . and .. */
	bool long_format;    /* -l long listing format */
	bool numeric_ids;       /*-n */
    bool classify;          /*-F */
    bool recursive;         /*-R */
    bool reverse;           /*-r */
    bool unsorted;          /* -f */
    bool sort_size;         /* -S */
    bool sort_time;         /* -t */
    bool use_atime;         /* -u */
    bool use_ctime;         /* -c */
    bool human_readable;    /* -h */
    bool kilobytes;         /* -k */
    bool inode;             /* -i */
    bool blocks;            /* -s */
    bool dir_as_file;       /* -d */
    bool printable_only;    /* -q */
};

/*file entry for storing directory contents*/
struct file_entry {
	char *name;
	struct stat sb;
};

/*declarations from ls.c*/
void ls_directory(const char *path, const struct options *opts);
void ls_file(const char *path, const struct options *opts);
void process_recursively(const char *path, const struct options *opts, bool print_name);

/*declarations from print.c*/
void print_filename_sanitized(const char *name);
void print_suffix(const struct stat *sb);
void print_size_column(const struct stat *sb, const struct options *opts);
void print_size_long(const struct stat *sb, const struct options *opts);
void print_long_format(const char *name, const struct stat *sb, const struct options *opts);
void print_simple(const char *name);
void print_columns(struct file_entry *entries, int count, const struct options *opts);

/*declarations from util.c*/
int compare_timem(const void *a, const void *b);
int compare_timea(const void *a, const void *b);
int compare_timec(const void *a, const void *b);
int compare_size(const void *a, const void *b);
void reverse_entries(struct file_entry *entries, int count);
bool is_directory(const char *path);
char *build_path(const char *dir, const char *file);
int compare_names(const void *a, const void *b);
uint64_t get_display_block_size(const struct stat *sb, const struct options *opts);
const char *format_size(uint64_t bytes, char *buf, size_t buflen);
void sort_entries(struct file_entry *entries, int count, const struct options *opts);

#endif /* !_LS_H_ */