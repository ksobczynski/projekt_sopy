#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#define COPY_BUFFER_SIZE 8192




void process_tree(char *source_base, char *target_base, const char* initial_base,const char* initial_target_base);
ssize_t buff_read(int fd, char* buf, ssize_t nbytes);
ssize_t buff_write(int fd,char*buf,ssize_t nbytes);
void copy_file(char* source, char* target, struct stat st);
int copy_content(int fd_src, int fd_target);
int is_dir_empty(const char *path);
int evaluate_symlink(char* source_symlink, const char* base_dir_src);
void copy_symlink(char* source, char* dest,char* source_base, char* target_base);
