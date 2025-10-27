/*print.c - output formatting funs for ls*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ls.h"

static int get_terminal_width(void);
static void print_time(time_t t);
void print_filename_sanitized(const char *name) {
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (isprint(*p)) {
            putchar(*p);
        }
		else {
            putchar('?');
        }
    }
}
/*print suffix for -F*/
void print_suffix(const struct stat *sb) {
    if (S_ISDIR(sb->st_mode)) {
        putchar('/');
    }
    else if (S_ISLNK(sb->st_mode)) {
        putchar('@');
    }
#ifdef S_ISWHT   
/*handles whiteout files, if they exist on the system it will do this, else it wont*/
    else if (S_ISWHT(sb->st_mode)) {
        putchar('%');
    }
#endif
    else if (S_ISSOCK(sb->st_mode)) {
        putchar('=');
    }
    else if (S_ISFIFO(sb->st_mode)) {
        putchar('|');
    }
    else if (S_ISREG(sb->st_mode)&&((sb->st_mode&S_IXUSR)||(sb->st_mode&S_IXGRP)||(sb->st_mode&S_IXOTH))) {
        putchar('*');
    }
}

/*PRINTS prefix size for -s*/
void print_size_column(const struct stat *sb, const struct options *opts) {
    if (opts->human_readable) {
        //-h
        char buf[16];
        uint64_t bytes = (uint64_t)sb->st_blocks * 512;
        format_size(bytes, buf, sizeof(buf));
        printf("%6s ", buf);
    } 
	else if (opts->kilobytes) {
        //-k: kilobytes
        unsigned long kb = (sb->st_blocks + 1) / 2; // round up
        printf("%4lu ", kb);
    } 
	else {
        // -s: block count
        printf("%4lu ", (unsigned long)sb->st_blocks);
    }
}
/*PRINTS sizes depending on flag*/
void print_size_long(const struct stat *sb, const struct options *opts) {
    if (opts->human_readable) {
        char buf[16];
        format_size((uint64_t)sb->st_size, buf, sizeof(buf));
        printf("%6s ", buf);
    }
    else {
        printf("%4lu ", (unsigned long)sb->st_blocks);
    }
}

/*Print file long format -l -n*/
void print_long_format(const char *name, const struct stat *sb, const struct options *opts){
	char mode_str[12];
	struct passwd *pw;
	struct group *gr;
	char *owner;
	char *group;
	if(opts->inode){
		printf("%9lu ", (unsigned long)sb->st_ino);
	}
	/*-s prefix print*/
	if(opts->blocks){
		print_size_column(sb, opts);
	}
	strmode(sb->st_mode, mode_str);
	(void)printf("%s", mode_str);
	/*number of links*/
	(void)printf(" %3lu", (unsigned long)sb->st_nlink);

	if((opts->long_format)&&!(opts->numeric_ids)){
		/*owner name*/
		if ((pw = getpwuid(sb->st_uid)) != NULL) {
			owner = pw->pw_name;
		} 
		else {
			static char uid_buf[32];
			(void)snprintf(uid_buf, sizeof(uid_buf), "%u", sb->st_uid);
			owner = uid_buf;
		}
		(void)printf(" %-8s", owner);

		/*group name*/
		if ((gr = getgrgid(sb->st_gid)) != NULL) {
			group = gr->gr_name;
		}
		else {
			static char gid_buf[32];
			(void)snprintf(gid_buf, sizeof(gid_buf), "%u", sb->st_gid);
			group = gid_buf;
		}
		(void)printf(" %-8s", group);
	}
	/*n case numerical uid/gid*/
	else{
		/*owner num*/
		(void)printf(" %u", sb->st_uid);
		/*group num*/
		(void)printf(" %u", sb->st_gid);
	}
	/*file size
	accounts for flag h*/
	if(opts->human_readable){
		print_size_long(sb, opts);
	}
	else{
		(void)printf(" %8lld", (long long)sb->st_size);
	}
	/*modification time*/
	(void)printf(" ");
	if(opts->use_atime){
		print_time(sb->st_atime);
	}
	else if(opts->use_ctime){
		print_time(sb->st_ctime);
	}
	else {print_time(sb->st_mtime);}
	putchar(' ');
	/*filename -q check*/
	if(opts->printable_only) {
		print_filename_sanitized(name);
	} 
	else {
		(void)printf(" %s", name);
	}

	/*print symlink destination*/
    if (S_ISLNK(sb->st_mode)) {
        char linkbuf[PATH_MAX];
        ssize_t len;
        
        len = readlink(name, linkbuf, sizeof(linkbuf) - 1);
        if (len != -1) {
            linkbuf[len] = '\0';
            printf(" -> %s", linkbuf);
        }
    }

	/*prints in case of -F flag*/
	if(opts->classify){
		print_suffix(sb);
	}
	(void)printf("\n");
}

/*print filename simple format. 
used when file is explicitly specified maybe adds*/
void print_simple(const char *name){
	(void)printf("%s\n", name);
}

/*print files in column*/
void print_columns(struct file_entry *entries, int count, const struct options *opts){
	int term_width;
	int max_len;
	int col_width;
	int num_cols;
	int num_rows;
	int row;
	int col;
	int idx;
	int i;

	if (count == 0) {
		return;
	}

	/*terminal width*/
	term_width = get_terminal_width();

	/*longest filename loop*/
	max_len = 0;
	for (i = 0; i < count; i++) {
		int len;
		len = strlen(entries[i].name);
		if (len > max_len) {
			max_len = len;
		}
	}
	col_width = max_len + 1;

	/*number of columns/rows that fit*/
	num_cols = term_width / col_width;
	if (num_cols<1) {
		num_cols=1;
	}
	num_rows = (count+num_cols-1)/num_cols;

	for (row = 0; row < num_rows; row++) {
		for (col = 0; col < num_cols; col++) {
			idx = col * num_rows + row;
			/*stop if no more files*/
			if (idx >= count) {
				break;
			}
			if(opts->inode){
				printf("%9lu ", (unsigned long)entries[idx].sb.st_ino);
			}
			/*print leading sz -s*/
			if(opts->blocks){
				print_size_column(&entries[idx].sb, opts);
			}
			/*filename -q check*/
			if (opts->printable_only) {
				print_filename_sanitized(entries[idx].name);
			} 
			else {
				(void)printf("%s", entries[idx].name);
			}
			/*prints in case of -F flag*/
			if(opts->classify){
				print_suffix(&entries[idx].sb);
			}
			/*padding unless last col*/
			if (col < num_cols - 1 && idx + num_rows < count) {
				int padding;
				int name_len;
				name_len = strlen(entries[idx].name);
				padding = col_width - name_len;
				while (padding>0) {
					(void)putchar(' ');
					padding--;
				}
			}
		}
		(void)putchar('\n');
	}
}

/*terminal width.*/
static int get_terminal_width(void) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		return ws.ws_col;
	}
	return 80;/*standard default terminal width, no macro available*/
}

/*formatted time.*/
static void print_time(time_t t) {
	char buf[64];
	struct tm *tm;
	if ((tm = localtime(&t)) != NULL) {
		(void)strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
		(void)printf("%s", buf);
	}
}