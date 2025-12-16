#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <limits.h>
#include <signal.h>


#define MAX_WATCHES 8192
#define EVENT_BUF_LEN (64 * (sizeof(struct inotify_event) + NAME_MAX + 1))
struct Watch {
    int wd; // key
    char *path; // value
};
struct WatchMap {
    struct Watch watch_map[MAX_WATCHES];
    int watch_count;
};
void update_watch_paths(struct WatchMap *map, const char *old_path, const char *new_path);
void add_to_map(struct WatchMap *map, int wd, const char *path);
struct Watch *find_watch(struct WatchMap *map, int wd);
void remove_from_map(struct WatchMap *map, int wd, int fd);
void add_watch_recursive(int notify_fd, struct WatchMap *map, const char *base_path);
void add_main(char* source_abs,char* dest_abs);
void usr1_action(int sig);
void set_signal_handling();
void remove_from_map_prefix(struct WatchMap* map, char* prefix, int fd);
void remove_dir_recursively(char* path);
void clear_dir(char*path);