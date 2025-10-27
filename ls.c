/*ls.c - main file*/

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <search.h>

#include "ls.h"

static void usage(void);
static void parse_options(int argc, char *argv[], struct options *opts);


/*entry for ls*/
int main(int argc, char *argv[]) {
	struct options opts;
	int i;
	bool has_args;

	/*init flags to false */
	opts.show_all = false;
	opts.long_format = false;

	/*parse flags*/
	parse_options(argc, argv, &opts);

	/*check for file/dir args*/
	has_args = (optind < argc);

	if (!has_args) {
		/*no args = current dir used*/
		if (opts.dir_as_file) {
			/*-d flag, show . as a file*/
			ls_file(".", &opts);
		} else if (opts.recursive) {
			process_recursively(".", &opts, false);
		} else {
			ls_directory(".", &opts);
		}
	}
	else {
		/*list each*/
		for (i = optind; i < argc; i++) {
			if (opts.dir_as_file) {
				//treat directories as plain files when -d
				ls_file(argv[i], &opts);
				continue;
			}
			if (is_directory(argv[i])) {
				if (opts.recursive) {
					process_recursively(argv[i], &opts, argc - optind > 1);
				} else {
					ls_directory(argv[i], &opts);
				}
			} 
			else {
				//when specific files named in args
				ls_file(argv[i], &opts);
			}
		}
	}
	return EXIT_SUCCESS;
}

/*parse flags*/
static void parse_options(int argc, char *argv[], struct options *opts) {
	int ch;
	opts->show_all=false;       /* -a all . files including . and .. */
	opts->show_almost_all=false;  /*-A all . files except . and .. */
	opts->long_format=false;    /* -l long listing format */
	opts->numeric_ids=false;       /*-n */
    opts->classify=false;          /*-F */
    opts->recursive=false;         /*-R */
    opts->reverse=false;           /*-r */
    opts->unsorted=false;          /* -f */
    opts->sort_size=false;         /* -S */
    opts->sort_time=false;         /* -t */
    opts->use_atime=false;         /* -u */
    opts->use_ctime=false;         /* -c */
    opts->human_readable=false;    /* -h */
    opts->kilobytes=false;         /* -k */
    opts->inode=false;             /* -i */
    opts->blocks=false;            /* -s */
    opts->dir_as_file=false;       /* -d */
	opts->printable_only=false;    /* -q */
	/*detect if output to terminal for -q/default behavior*/
	if (isatty(STDOUT_FILENO)) {
		opts->printable_only=true;
	}
	/*sets -A flag if superuser*/
	if (geteuid() == 0) {
		opts->show_almost_all=true;
	}
	while ((ch = getopt(argc, argv, "-AacdFfhiklnqRrSstuw")) != -1) {
		switch (ch) {
		case 1:
        	/*printf("ls: unknown option -- %d\n", ch);*/
			usage();
        break; 
		case 'a':
			opts->show_all = true;
			break;
		case 'A':
			opts->show_almost_all = true;
			break;
		case 'l':
			opts->long_format = true;
			break;
		case 'n':
			opts->numeric_ids = true;
			break;
		case 'F':
        	opts->classify = true;
        	break;
		case 'R':
			opts->recursive = true;
			break;
		case 'r':
			opts->reverse = true;
			break;
		case 'f':
			opts->unsorted = true;
			break;
		case 'S':
			opts->sort_size = true;
			opts->use_atime = false;
			opts->use_ctime = false;
			opts->sort_time = false;
			break;
		case 't':
			opts->sort_time = true;
			opts->sort_size = false;
			break;
		case 'u':
			opts->use_atime = true;
			opts->use_ctime = false;
			opts->sort_size = false;
			break;
		case 'c':
			opts->use_ctime = true;
			opts->use_atime = false;
			opts->sort_size = false;
			break;
		case 'h':
			opts->human_readable = true;
			opts->kilobytes = false;
			break;
		case 'k':
			opts->kilobytes = true;
			opts->human_readable = false;
			break;
		case 'i':
			opts->inode = true;
			break;
		case 's':
			opts->blocks = true;
			break;
		case 'd':
			opts->dir_as_file = true;
			break;
		case 'q':
			opts->printable_only = true;
			break;
		case 'w':
			opts->printable_only = false;
			break;
		default:
			fprintf(stderr, "ls: unknown option -- %d\n", ch);
			usage();
			exit(EXIT_FAILURE);
		}
	}
}

/*list contents of a dir*/
void ls_directory(const char *path, const struct options *opts){
	DIR *dir;
	struct dirent *entry;
	struct file_entry *files;
	char *fullpath;
	int capacity;
	int count;
	int i;
	uint64_t total_blocks = 0;
	uint64_t total_size_bytes = 0;
	
	if ((dir = opendir(path)) == NULL) {
		warn("cannot access '%s'", path);
		return;
	}

	/*alloc initial array for files*/
	capacity = 64;
	count = 0;
	if ((files = malloc(capacity * sizeof(struct file_entry))) == NULL) {
		err(1, NULL);
	}

	/*read all dir entries*/
	while ((entry = readdir(dir)) != NULL) {
		/*skip . and .. unless -a*/
		/*need to check for A, so if A exclude . and ..
		keep the rest*/
		if (!(opts->show_all)) {
			if(opts->show_almost_all){
				if (!((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))) {
					goto dirbody;
				}
			}
			if (entry->d_name[0] == '.') {
				continue;
			}
		}
		dirbody:
		/*expand array if needed*/
		if (count >= capacity) {
			struct file_entry *new_files;

			capacity *= 2;
			new_files = realloc(files, 
			    capacity * sizeof(struct file_entry));
			if (new_files == NULL) {
				err(1, NULL);
			}
			files = new_files;
		}

		/*filename*/
		if ((files[count].name = strdup(entry->d_name)) == NULL) {
			err(1, NULL);
		}

		/*get file stats if needed*/
		if ((opts->long_format)||(opts->numeric_ids)||(opts->blocks)||(opts->classify)||(opts->inode)||(opts->sort_time)||(opts->sort_size)) {
			fullpath = build_path(path, entry->d_name);
			if (fullpath == NULL) {
				err(1, NULL);
			}
			if (lstat(fullpath, &files[count].sb) < 0) {
				warn("cannot stat '%s'", fullpath);
				free(fullpath);
				free(files[count].name);
				continue;
			}
			free(fullpath);
			//Acc logical file sizes in bytes
			total_size_bytes += files[count].sb.st_size;
			total_blocks += files[count].sb.st_blocks;
		}
		count++;
	}
	//printf("en %luK\n", total_size_bytes);
	closedir(dir);
	/*print total count on top*/
	if ((opts->long_format)||(opts->numeric_ids)||((opts->blocks))) {
		if (opts->human_readable) {
			// Convert to kilobytes, rounding up
			uint64_t total_kb = (total_size_bytes + 1023) / 1024;
    		printf("total %luK\n", total_kb);
		} else if (opts->kilobytes) {
			//char buf[16];
			// total_blocks is in 512-byte units, convert to bytes
			printf("total %lu\n", (total_blocks + 1) / 2); 
		} else {
			// Default: show raw 512-byte block count
			printf("total %lu\n", total_blocks);
		}
	}
	/* HERE IS WHERE IMPLEMENTED SORTING STUFF, WONT MESS WITH OTHER FLAGS!! */
	if (!opts->unsorted) {
		if (opts->sort_time) {
			if(opts->use_atime){
			qsort(files, count, sizeof(struct file_entry), compare_timea);
			}
			else if(opts->use_ctime){
				qsort(files, count, sizeof(struct file_entry), compare_timec);
			}
			else {
				qsort(files, count, sizeof(struct file_entry), compare_timem);
			}
		} 
		else if (opts->sort_size) {
			qsort(files, count, sizeof(struct file_entry), compare_size);
		} 
		else {
			qsort(files, count, sizeof(struct file_entry), compare_names);
		}
		
		/* Reverse if -r flag */
		if (opts->reverse) {
			reverse_entries(files, count);
		}
	}
	/*display files*/
	if ((opts->long_format)||(opts->numeric_ids)) {
		/*long -l or n, one file per line with details*/
		for (i = 0; i < count; i++) {
			print_long_format(files[i].name, &files[i].sb, opts);
		}
	} else {
		/*simple form with columns*/
		print_columns(files, count, opts);
	}

	for (i = 0; i < count; i++) {
		free(files[i].name);
	}
	free(files);
}
/*process directory recursively.
lists current directory, then recurses into subdirectories.*/
void process_recursively(const char *path, const struct options *opts, bool print_name) {
	DIR *dir;
	struct dirent *entry;
	struct file_entry *subdirs;
	char *fullpath;
	struct stat sb;
	int capacity;
	int count;
	int i;

	/*directory name if*/
	if (print_name) {
		(void)printf("%s:\n", path);
	}
	/*current directory listed by ls_directory */
	ls_directory(path, opts);

	/*collect subdirs for recursion*/
	if ((dir = opendir(path)) == NULL) {
		warn("cannot access '%s'", path);
		return;
	}

	/*array for subdir entries*/
	capacity = 16;
	count = 0;
	if ((subdirs = malloc(capacity * sizeof(struct file_entry))) == NULL) {
		err(1, NULL);
	}

	/*read directory and collect subdirectories*/
	while ((entry = readdir(dir)) != NULL) {
		/* Skip . and .. */
		if (strcmp(entry->d_name, ".") == 0 || 
		    strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		/*skip hidden files unless -a */
		if (!opts->show_all && entry->d_name[0] == '.') {
			continue;
		}
		fullpath = build_path(path, entry->d_name);
		if (fullpath == NULL) {
			err(1, NULL);
		}

		if (lstat(fullpath, &sb) < 0) {
			warn("cannot stat '%s'", fullpath);
			free(fullpath);
			continue;
		}

		/*skip sym links*/
		if (S_ISLNK(sb.st_mode)) {
			free(fullpath);
			continue;
		}

		if (S_ISDIR(sb.st_mode)) {
			if (count >= capacity) {
				struct file_entry *new_subdirs;

				capacity *= 2;
				new_subdirs = realloc(subdirs, 
				    capacity * sizeof(struct file_entry));
				if (new_subdirs == NULL) {
					err(1, NULL);
				}
				subdirs = new_subdirs;
			}

			subdirs[count].name = fullpath;
			subdirs[count].sb = sb;
			count++;
		} else {
			free(fullpath);
		}
	}

	closedir(dir);

	/*subdirs sorted according to flags*/
	sort_entries(subdirs, count, opts);

	/*recurse subdirs in order */
	for (i = 0; i < count; i++) {
		(void)printf("\n");
		process_recursively(subdirs[i].name, opts, true);
		free(subdirs[i].name);
	}

	free(subdirs);
}

/*list a single file*/
void ls_file(const char *path, const struct options *opts) {
	struct stat sb;
	if (lstat(path, &sb) < 0) {
		warn("cannot access '%s'", path);
		return;
	}
	if ((opts->long_format)||(opts->numeric_ids)) {
		print_long_format(path, &sb, opts);
	} else {
		print_simple(path);
	}
}

static void usage(void){
	(void)fprintf(stderr, "usage: ls [-al] [file ...]\n");
	exit(EXIT_FAILURE);
}