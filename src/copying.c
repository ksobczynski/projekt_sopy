#include "copying.h"

ssize_t buff_read(int fd, char* buf, ssize_t nbytes)
{
    ssize_t len = 0;
    ssize_t c;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd,buf,nbytes));
        if (c==0)
            return len;
        if (c<0)
            return c;
        buf +=c;
        len += c;
        nbytes -= c;
    } while (nbytes>0);
    return len;
}

ssize_t buff_write(int fd,char*buf,ssize_t nbytes)
{
    ssize_t len = 0;
    ssize_t c;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd,buf,nbytes));
        if (c<0)
            return c;
        if (c>nbytes)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        buf +=c;
        len += c;
        nbytes -= c;
    } while (nbytes>0);
    return len;
}

int copy_content(int fd_src, int fd_target) {
    char buffer[COPY_BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_written;

    while (1) {

        bytes_read = buff_read(fd_src, buffer, COPY_BUFFER_SIZE);
        if (bytes_read < 0) {
            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        bytes_written = buff_write(fd_target, buffer, bytes_read);

        if (bytes_written < 0) {
            return -1;
        }

        if (bytes_written != bytes_read) {
            fprintf(stderr, "Cricital error copied wrong amount of bytes!\n");
            return -1;
        }
    }
    return 0;
}
int is_dir_empty(const char *path) {
    DIR *d = opendir(path);
    if (!d) return -1;

    struct dirent *dir;
    int is_empty = 1;

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            is_empty = 0;
            break;
        }
    }
    closedir(d);
    return is_empty;
}
void copy_file(char* source, char* target, struct stat st)
{
    int fd_src = open(source,O_RDONLY);
    if(fd_src==-1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    int fd_target = open(target,O_CREAT | O_RDWR | O_TRUNC, st.st_mode);
    if (fd_target == -1) {
            perror("open");
            return;
    }
    fchmod(fd_target,st.st_mode);
    if(copy_content(fd_src,fd_target)==-1)
    {
        perror("copy");
        exit(EXIT_FAILURE);
    }
    if(close(fd_src)==-1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
    if(close(fd_target)==-1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }
}
void copy_symlink(char* source, char* dest, char* src_base, char* target_base)
{
    struct stat st;
    if(lstat(source, &st) == -1) {
        perror("lstat");
        return;
    }

    if(!S_ISLNK(st.st_mode)) {
        return;
    }

    char link_target[PATH_MAX];
    ssize_t len = readlink(source, link_target, sizeof(link_target) - 1);

    if (len != -1) {
        link_target[len] = '\0';

        char fixed_link_target[PATH_MAX];
        char *final_target = link_target;

        size_t src_len = strlen(src_base);

        if (strlen(link_target) >= src_len && strncmp(link_target, src_base, src_len) == 0)
        {
            char boundary = link_target[src_len];
            if (boundary == '\0' || boundary == '/')
            {
                snprintf(fixed_link_target, PATH_MAX, "%s%s", target_base, link_target + src_len);

                final_target = fixed_link_target;
            }
        }
        if(symlink(final_target, dest) == -1)
        {
            perror("symlink");
        }
    } else {
        perror("readlink");
    }
}
/*
 * in: na co wskazuje symlink - source_symlink,
 * in: bazowa sciezka directory - base_dir_src
 *
 * out: 1 - symlink wskazuje na cos wewnatrz ogladanego drzewa, 0 - symlink wskazuja na cos spoza
 * */

int evaluate_symlink(char* source_symlink, const char* base_dir_src)
{
    if (source_symlink[0] != '/') { //TODO sprawdz czy nie aktualna sciezka
        return 0;
    }
    char* result = strstr(source_symlink,base_dir_src);
    if(result)
        return 1;
    return 0;
}
/*
 * in:
 *      char* source_base - sciezka do pliku zrodlowego z ktorego chcemy kopiowac
 *      char* target_base - sciezka do pliku docelowego do ktorego bedziemy kopiowac
 *      const char* initial_base - sciezka do bazy pliku zrodlowego - potrzebna do rozpoznania na glebokiej rekurencji
 *      od jakiego drzewa zaczynalismy
 *
 * */
void process_tree(char *source_base, char *target_base, const char* initial_base, const char* initial_target_base) {
    DIR *d;
    struct dirent *dir;
    struct stat st;
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    d = opendir(source_base);
    if (!d) { // czy to dir i czy istnieje
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    if(stat(source_base,&st)==-1) // wezmy sb dane o src
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    errno=0;
    if (mkdir(target_base, st.st_mode) == -1) {
        if (errno == EEXIST) {
            int empty = is_dir_empty(target_base);

            if (empty == 0) {
                fprintf(stderr, "Error: Dir '%s' exists and is not empty!\n", target_base);
                exit(EXIT_FAILURE);
            } else if (empty == -1) {
                perror("Error checking target directory");
                exit(EXIT_FAILURE);
            }

            chmod(target_base, st.st_mode);

        } else {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }

    while ((dir = readdir(d)) != NULL) {
        strcpy(source_path,source_base);
        strcpy(target_path,target_base);
        strcat(source_path, "/");
        strcat(target_path, "/");

        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) { // ignoruje . i ..
            continue;
        }

        strcat(source_path,dir->d_name);
        strcat(target_path,dir->d_name);

        if (lstat(source_path, &st) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            process_tree(source_path, target_path, initial_base,initial_target_base);
        } else if (S_ISREG(st.st_mode)) {
            copy_file(source_path,target_path,st);
        } else if (S_ISLNK(st.st_mode)) {

            char link_target[PATH_MAX];
            ssize_t len = readlink(source_path, link_target, sizeof(link_target) - 1);

            if (len != -1) {
                link_target[len] = '\0';
                if(evaluate_symlink(link_target,initial_base)==0)
                {
                    if (symlink(link_target, target_path) == -1) {
                        perror("symlink failed");
                        // opcjonalnie: exit(EXIT_FAILURE);
                    }
                }
                else{
                    char fixed_path[PATH_MAX];
                    strcpy(fixed_path,initial_target_base);
                    // strcat(fixed_path,"/"); // /home/etc
                    strcat(fixed_path,link_target+strlen(initial_base));
                    if (symlink(fixed_path, target_path) == -1) {
                        perror("symlink failed");
                    }
                }
            } else {
                perror("readlink failed");
            }
            }
    }
    closedir(d);
}
