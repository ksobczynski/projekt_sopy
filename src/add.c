#include "copying.h"
#include "add.h"

volatile sig_atomic_t last_sig = 0;
void add_to_map(struct WatchMap *map, int wd, const char *path) {
    if (map->watch_count >= MAX_WATCHES) {
        fprintf(stderr, "Exceeded max watches!\n");
        return;
    }
    map->watch_map[map->watch_count].wd = wd;
    map->watch_map[map->watch_count].path = strdup(path);
    map->watch_count++;
}

struct Watch *find_watch(struct WatchMap *map, int wd) {
    for (int i = 0; i < map->watch_count; i++) {
        if (map->watch_map[i].wd == wd) {
            return &map->watch_map[i];
        }
    }
    return NULL;
}

void remove_from_map(struct WatchMap *map, int wd, int fd) {
    for (int i = 0; i < map->watch_count; i++) {
        if (map->watch_map[i].wd == wd) {
            free(map->watch_map[i].path);
            map->watch_map[i] = map->watch_map[map->watch_count - 1];
            map->watch_count--;

            return;
        }
    }
}
void clear_dir(char*path)
{
    DIR* d = opendir(path);
    if (!d) {
        return;
    }

    struct dirent *dir;
    struct stat st;

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", path, dir->d_name);

        if (lstat(file_path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_dir_recursively(file_path);
        } else {
            unlink(file_path);
        }
    }

    closedir(d);
}
void remove_dir_recursively(char* path)
{
    DIR* d = opendir(path);
    if (!d) {
        return;
    }

    struct dirent *dir;
    struct stat st;

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", path, dir->d_name);

        if (lstat(file_path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_dir_recursively(file_path);
        } else {
            unlink(file_path);
        }
    }

    closedir(d);

    if(rmdir(path) == -1) {
        perror("rmdir");
    }

}

void remove_from_map_prefix(struct WatchMap* map, char* prefix, int fd)
{
    size_t prefix_len = strlen(prefix);
    int i = 0;

    while (i < map->watch_count) {
        char *current_path = map->watch_map[i].path;
        size_t current_len = strlen(current_path);
        if (current_len < prefix_len) {
            i++;
            continue;
        }
        if (strncmp(current_path, prefix, prefix_len) == 0) {
            if (current_path[prefix_len] == '\0' || current_path[prefix_len] == '/') {


                inotify_rm_watch(fd, map->watch_map[i].wd);
                free(map->watch_map[i].path);

                map->watch_map[i] = map->watch_map[map->watch_count - 1];
                map->watch_count--;

                continue;
            }
        }

        i++;
    }
}

void add_watch_recursive(int notify_fd, struct WatchMap *map, const char *base_path) {
    uint32_t mask = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;

    int wd = inotify_add_watch(notify_fd, base_path, mask);
    if (wd < 0) {
        perror("inotify_add_watch");
        return;
    }
    add_to_map(map, wd, base_path);

    DIR *dir = opendir(base_path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            add_watch_recursive(notify_fd, map, full_path);
        }
    }

    closedir(dir);
}

void clean_map(struct WatchMap *map)
{
    for(int i=map->watch_count-1; i>0;i--)
    {
        free(map->watch_map[i].path);
    }
}
//TODO evaluate czy potrzebne
void update_watch_paths(struct WatchMap *map, const char *old_path, const char *new_path) { // /A/B/C/test.txt /A/X/C/
    size_t old_len = strlen(old_path);

    for (int i = 0; i < map->watch_count; i++) {
        if (strncmp(map->watch_map[i].path, old_path, old_len) == 0 && (
                map->watch_map[i].path[old_len] == '/' || map->watch_map[i].path[old_len] == '\0')) {
            char new_full_path[PATH_MAX];
            // Sklej nową ścieżkę z resztą (sufiksem) starej ścieżki
            snprintf(new_full_path, sizeof(new_full_path), "%s%s",
                     new_path, map->watch_map[i].path + old_len);

            free(map->watch_map[i].path);
            map->watch_map[i].path = strdup(new_full_path);
            i--;
        }
    }
}
void usr1_action(int sig)
{
    last_sig = SIGUSR1;
}
void set_signal_handling()
{
    struct sigaction sa;
    memset(&sa,0,sizeof(struct sigaction));
    sa.sa_handler = usr1_action;
    sigaction(SIGUSR1,&sa,NULL);
}
void add_main(char* source_abs, char* dest_abs) // np /dane /backup
{
    set_signal_handling();
    if(strlen(source_abs) == 0 || strlen(dest_abs) == 0)
    {
        printf("Please provide accurate path!\n");
        return;
    }

    process_tree(source_abs, dest_abs, source_abs, dest_abs);

    int notify_fd = inotify_init();
    if (notify_fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    struct WatchMap map = {0};

    add_watch_recursive(notify_fd, &map, source_abs);


    while (map.watch_count > 0 && last_sig != SIGUSR1) {
        char buffer[EVENT_BUF_LEN];
        errno = 0;
        ssize_t len = read(notify_fd, buffer, EVENT_BUF_LEN);

        if (len < 0) {
            if(errno == EINTR)
            {
                if(last_sig==SIGUSR1)
                {
                    goto finish;
                }
            }
            else{
                printf("Error reading from buffer. Exiting...\n");
                goto finish;
            }

        }

        ssize_t i = 0;
        while (i < len) {
            if(last_sig==SIGUSR1)
                goto finish;
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            struct Watch *watch = find_watch(&map, event->wd);

            char event_path[PATH_MAX] = "";
            char target_event_path[PATH_MAX];
            memset(event_path,0,PATH_MAX);
            memset(target_event_path,0,PATH_MAX);
            if (watch && event->len > 0) {
                snprintf(event_path, sizeof(event_path), "%s/%s", watch->path, event->name);
                snprintf(target_event_path,PATH_MAX,"%s/%s",dest_abs,event_path+strlen(source_abs)+1);
            } else if (watch) {
                strncpy(event_path, watch->path, sizeof(event_path));
                snprintf(target_event_path,PATH_MAX,"%s",dest_abs);
            }
            else{
                i += sizeof(struct inotify_event) + event->len;
                continue;
            }
            struct stat event_file_stat;


            size_t name_len = strlen(event->name);
            if (event->mask & IN_IGNORED) {
                i += sizeof(struct inotify_event) + event->len;
                remove_from_map(&map, event->wd, notify_fd);
                continue;
            }
            else if (name_len > 0 && event->name[name_len - 1] == '~') {
                // To jest plik tymczasowy -> ignorujemy

                i += sizeof(struct inotify_event) + event->len;
                continue;
            }
            else if (event->mask & IN_ISDIR) {

                if (event->mask & IN_CREATE) { // stworzono podfolder
                    errno = 0;
                    if(lstat(event_path,&event_file_stat)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",event_path);
                            goto finish;
                        }
                        perror("lstat");
                        break;
                    }

                    process_tree(event_path,target_event_path,event_path,target_event_path);
                    add_watch_recursive(notify_fd, &map, event_path);
                } if (event->mask & IN_DELETE) { // usunieto podfolder
                    if(rmdir(target_event_path)==-1) // podfolder musi byc pusty bo najpierw przyjdzie info o usunietych dzieciach
                    {
                        errno=0;
                        if(errno == ENOENT)
                        {
                            printf("Directory '%s' doesnt. Exiting Watch...\n",target_event_path);
                            goto finish;
                        }
                        perror("rmdir");
                        break;
                    }
                } else if (event->mask & IN_MOVED_FROM) {
                    // remove dir recursively
                    remove_from_map_prefix(&map,event_path,notify_fd);
                    remove_dir_recursively(target_event_path);
                } else if (event->mask & IN_MOVED_TO) {
                    process_tree(event_path,target_event_path,event_path,target_event_path);
                    add_watch_recursive(notify_fd, &map, event_path);
                }

                printf("\n");
            } else {

                if (event->mask & IN_CLOSE_WRITE) {
                    errno=0;
                    if(lstat(event_path,&event_file_stat)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",event_path);
                            goto finish;
                        }
                        perror("lstat");
                        break;
                    }
                    copy_file(event_path,target_event_path,event_file_stat);
                }
                if (event->mask & IN_CREATE && !(event->mask & IN_ISDIR)) {
                    errno=0;
                    if(lstat(event_path,&event_file_stat)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",event_path);
                            goto finish;
                        }

                        perror("lstat");
                        break;
                    }
                    if(S_ISLNK(event_file_stat.st_mode))
                    {
                        copy_symlink(event_path,target_event_path,source_abs,dest_abs);
                    }
                    else if(open(target_event_path,O_CREAT | O_RDWR,event_file_stat.st_mode)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("Error while creating '%s'. Exiting Watch...\n",target_event_path);
                            goto finish;
                        }
                        perror("open");
                        break;
                    }
                }
                if (event->mask & IN_DELETE && !(event->mask & IN_ISDIR)) {
                    errno=0;
                    if(unlink(target_event_path)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",target_event_path);
                            goto finish;
                        }
                        perror("unlink");
                        break;
                    }
                }
                if (event->mask & IN_MOVED_FROM) {
                    if(unlink(target_event_path)==-1)
                    {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",target_event_path);
                            goto finish;
                        }
                        perror("unlink");
                        break;
                    }
                }
                if (event->mask & IN_MOVED_TO && !(event->mask & IN_ISDIR)) {
                    errno=0;
                    if(lstat(event_path,&event_file_stat)==-1) {
                        if(errno == ENOENT)
                        {
                            printf("File '%s' doesnt exist. Exiting Watch...\n",event_path);
                            goto finish;
                        }
                        perror("lstat");
                        break;
                    }
                    if(S_ISLNK(event_file_stat.st_mode))
                    {
                        copy_symlink(event_path,target_event_path,source_abs,dest_abs);
                    }
                    else{

                        copy_file(event_path,target_event_path,event_file_stat);
                    }

                }

            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }
    finish:
    clean_map(&map);
    close(notify_fd);
    exit(EXIT_SUCCESS);
}
