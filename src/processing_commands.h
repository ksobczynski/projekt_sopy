#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#define MAX_SUBPROCESSES 30
struct Child
{
    pid_t child_pid;
    char* source;
    char* target;
};

struct ChildMap
{
    struct Child child_map[MAX_SUBPROCESSES];
    int children;
};
#define MAX_ARGS 80
#define PATH_MAX 4096
void handle_command(char* command,struct ChildMap* childmap,struct ChildMap* unactive, int* end);
void delete_map(struct ChildMap* child_map);
void handle_exit(char* command, int* end);
void handle_list(char* command,struct ChildMap* child_map,struct ChildMap* unactive);
void handle_add(char* command,struct ChildMap* childmap);
void handle_end(char* command,struct ChildMap* childmap,struct ChildMap* unactive);
void handle_restore(char* command,struct ChildMap* childmap,struct ChildMap* unactive);
void remove_from_child_map(struct ChildMap *map, char* target);
void add_to_child_map(struct ChildMap *map, pid_t pid,const char* source ,const char *target);
int validate_watch(char* source_path,char* target_path,struct ChildMap* childmap);
struct Child* find_child(struct ChildMap *map,const char *target);
void remove_from_child_map_pid(struct ChildMap *map, pid_t pid);
void end_main(char* src, char* dest, struct ChildMap* child_map,struct ChildMap* unactive);
int check_forbidden_chars(char* path);
void restore_main(char* source, char* target,struct ChildMap* child_map);