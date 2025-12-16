#include "processing_commands.h"
#include "add.h"
#include "copying.h"

void remove_from_child_map(struct ChildMap *map, char* target) {
    for (int i = 0; i < map->children; i++) {
        if (strcmp(map->child_map[i].target,target)==0) {
            printf("removing backup: '%s' -> '%s'\n", map->child_map[i].source,map->child_map[i].target);
            free(map->child_map[i].target);
            free(map->child_map[i].source);
            map->child_map[i] = map->child_map[map->children - 1];
            map->children--;
            return;
        }
    }
}
void add_to_child_map(struct ChildMap *map, pid_t pid, const char* source ,const char *target) {
    if (map->children >= MAX_SUBPROCESSES) {
        fprintf(stderr, "Exceeded max watches!\n");
        return;
    }
    map->child_map[map->children].child_pid = pid;
    map->child_map[map->children].source = strdup(source);
    map->child_map[map->children].target = strdup(target);
    map->children++;
    printf("new watch: '%s' -> '%s\n", source,target);
}
struct Child* find_child(struct ChildMap *map,const char *target)
{
    for(int i=0;i<map->children;i++)
    {
        if(strcmp(map->child_map[i].target, target)==0)
        {
            return &map->child_map[i];
        }
    }
    return NULL;
}
void handle_command(char* command, struct ChildMap* childmap,struct ChildMap* unactive,int* end)
{
    char word[PATH_MAX];
    int i=0;
    while(command[i] != '\n' && command[i] != ' ')
    {
        word[i] = command[i];
        i++;
    }
    word[i] = '\0';
    if(strcmp(word,"add")==0)
        handle_add(command+i+1,childmap);
    else if(strcmp(word,"list")==0)
        handle_list(command+i+1,childmap,unactive);
    else if(strcmp(word,"end")==0)
        handle_end(command+i+1,childmap,unactive);
    else if(strcmp(word,"restore")==0)
        handle_restore(command+i+1,childmap,unactive);
    else if(strcmp(word,"exit")==0)
        handle_exit(command+i+1,end);
    else{
        printf("command %s is not valid\n",word);
        return;
    }
}

void handle_exit(char* command, int* end)
{
    if(strtok(command," \n") != NULL)
    {
        printf("Command \'exit\' only takes one argument\n");
        return;
    }
    *end = 1;
}

void handle_add(char* command, struct ChildMap* childmap)
{
    char source_path[PATH_MAX];
    char target_paths[MAX_ARGS][PATH_MAX];

    memset(source_path, 0, sizeof(source_path));
    memset(target_paths, 0, sizeof(target_paths));

    int isSource = 1;
    int quotMark = 0;
    int targets_count = 0;

    char* delim = " \n";
    char* word = strtok(command, delim);

    char *current_buffer = source_path;

    while(word != NULL)
    {
        int len = strlen(word);

        if (quotMark) {
            strcat(current_buffer, " ");

            if (word[len - 1] == '\"') {
                strncat(current_buffer, word, len - 1);

                quotMark = 0;

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_paths[targets_count];
                } else {
                    targets_count++;
                    if(targets_count < MAX_ARGS)
                        current_buffer = target_paths[targets_count];
                }
            } else {
                strcat(current_buffer, word);
            }
        }
        else {
            if (word[0] == '\"') {
                if (len > 1 && word[len - 1] == '\"') {
                    strncat(current_buffer, word + 1, len - 2);

                    if (isSource) {
                        isSource = 0;
                        current_buffer = target_paths[targets_count];
                    } else {
                        targets_count++;
                        if(targets_count < MAX_ARGS)
                            current_buffer = target_paths[targets_count];
                    }
                } else {
                    strcat(current_buffer, word + 1);
                    quotMark = 1;
                }
            } else {
                strcat(current_buffer, word);

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_paths[targets_count];
                } else {
                    targets_count++;
                    if(targets_count < MAX_ARGS)
                        current_buffer = target_paths[targets_count];
                }
            }
        }

        word = strtok(NULL, delim);
    }
    if(targets_count<1)
    {
        printf("Too little arguments for function add\n");
        return;
    }
    if(check_forbidden_chars(source_path)==-1)
    {
        printf("Source is not of valid format: '%s'\n", source_path);
        return;
    }
    for(int j = 0; j < targets_count; j++)
    {
        if(check_forbidden_chars(target_paths[j])==-1)
        {
            printf("Target is not of valid format: '%s'\n", target_paths[j]);
            continue;
        }
        if (strlen(target_paths[j]) == 0) continue;
        if(validate_watch(source_path,target_paths[j],childmap)==-1)
        {
            printf("Wrong form of input for '%s' -> '%s'\n", source_path,target_paths[j]);
            continue;
        }
        printf("Will be adding from src: '%s' to target: '%s'\n", source_path, target_paths[j]);
        DIR *d;
        struct stat st;

        d = opendir(source_path);
        if (!d) { // czy to dir i czy istnieje
            printf("Source doesn\'t exist\n");
            break;
        }


        if(stat(source_path,&st)==-1) // wezmy sb dane o src
        {
            perror("stat");
            continue;
        }
        if(closedir(d)==-1)
        {
            perror("closedir");
            continue;
        }

        errno=0;
        if (mkdir(target_paths[j], st.st_mode) == -1) {
            if (errno == EEXIST) {
                int empty = is_dir_empty(target_paths[j]);

                if (empty == 0) {
                    fprintf(stderr, "Error: Directory '%s' exists and is not empty!\n", target_paths[j]);
                    continue;
                } else if (empty == -1) {
                    perror("mkdir");
                    continue;
                }

                chmod(target_paths[j], st.st_mode);

            } else {
                perror("mkdir");
                continue;
            }
        }

        char src_abs[PATH_MAX];
        char dest_abs[PATH_MAX];
        memset(src_abs,0,PATH_MAX);
        memset(dest_abs,0,PATH_MAX);
        if (!realpath(source_path, src_abs)) {
            perror("realpath source");
            break;
        }
        if (!realpath(target_paths[j], dest_abs)) {
            perror("realpath dest");
            continue;
        }
        pid_t kid_pid = fork();
        if (kid_pid == -1) {
            perror("fork");
            continue;
        }
        if (kid_pid == 0) {
            add_main(src_abs, dest_abs);
            exit(EXIT_SUCCESS);
        }
        else {
            add_to_child_map(childmap, kid_pid, src_abs, dest_abs);
        }
    }


}
void handle_end(char* command, struct ChildMap* childmap,struct ChildMap* unactive)
{
    char source_path[PATH_MAX];
    char target_paths[MAX_ARGS][PATH_MAX];

    memset(source_path, 0, sizeof(source_path));
    memset(target_paths, 0, sizeof(target_paths));

    int isSource = 1;
    int quotMark = 0;
    int targets_count = 0;

    char* delim = " \n";
    char* word = strtok(command, delim);

    char *current_buffer = source_path;

    while(word != NULL)
    {
        int len = strlen(word);

        if (quotMark) {
            strcat(current_buffer, " ");

            if (word[len - 1] == '\"') {
                strncat(current_buffer, word, len - 1);

                quotMark = 0;

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_paths[targets_count];
                } else {
                    targets_count++;
                    if(targets_count < MAX_ARGS)
                        current_buffer = target_paths[targets_count];
                }
            } else {
                strcat(current_buffer, word);
            }
        }
        else {
            if (word[0] == '\"') {
                if (len > 1 && word[len - 1] == '\"') {
                    strncat(current_buffer, word + 1, len - 2);

                    if (isSource) {
                        isSource = 0;
                        current_buffer = target_paths[targets_count];
                    } else {
                        targets_count++;
                        if(targets_count < MAX_ARGS)
                            current_buffer = target_paths[targets_count];
                    }
                } else {
                    strcat(current_buffer, word + 1);
                    quotMark = 1;
                }
            } else {
                strcat(current_buffer, word);

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_paths[targets_count];
                } else {
                    targets_count++;
                    if(targets_count < MAX_ARGS)
                        current_buffer = target_paths[targets_count];
                }
            }
        }

        word = strtok(NULL, delim);
    }
    if(targets_count<1)
    {
        printf("Too little arguments for function end\n");
        return;
    }
    if(check_forbidden_chars(source_path)==-1)
    {
        printf("Source is not of valid format: '%s'\n", source_path);
        return;
    }
    for(int j=0;j<targets_count;j++)
    {
        if(check_forbidden_chars(target_paths[j])==-1)
        {
            printf("Target is not of valid format: '%s'\n", target_paths[j]);
            continue;
        }
        char src_abs[PATH_MAX];
        char dest_abs[PATH_MAX];
        memset(src_abs,0,PATH_MAX);
        memset(dest_abs,0,PATH_MAX);
        errno = 0;
        if(!realpath(source_path,src_abs))
        {
            if(errno == EEXIST)
            {
                printf("'%s' doesn\'t exist", source_path);
                break;
            }
        }
        if(!realpath(target_paths[j],dest_abs))
        {
            if(errno == EEXIST)
            {
                printf("'%s' doesn\'t exist", target_paths[j]);
                continue;
            }
        }
        end_main(src_abs,dest_abs,childmap, unactive);
    }

}
void handle_restore(char* command,struct ChildMap* childmap,struct ChildMap* unactive)
{
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    memset(source_path, 0, sizeof(source_path));
    memset(target_path, 0, sizeof(target_path));

    int isSource = 1;
    int targetDone = 0;
    int quotMark = 0;

    char* delim = " \n";
    char* word = strtok(command, delim);

    char *current_buffer = source_path;

    while(word != NULL)
    {
        int len = strlen(word);
        if (targetDone)
        {
            printf("Too long input for function restore\n");
            return;
        }
        if (quotMark) {
            strcat(current_buffer, " ");

            if (word[len - 1] == '\"') {
                strncat(current_buffer, word, len - 1);

                quotMark = 0;

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_path;
                }
                else{
                    targetDone=1;
                }
            } else {
                strcat(current_buffer, word);
            }
        }
        else {
            if (word[0] == '\"') {
                if (len > 1 && word[len - 1] == '\"') {
                    strncat(current_buffer, word + 1, len - 2);

                    if (isSource) {
                        isSource = 0;
                        current_buffer = target_path;
                    } else {
                        targetDone=1;
                    }
                } else {
                    strcat(current_buffer, word + 1);
                    quotMark = 1;
                }
            } else {
                strcat(current_buffer, word);

                if (isSource) {
                    isSource = 0;
                    current_buffer = target_path;
                } else {
                    targetDone=1;
                }
            }
        }
        word = strtok(NULL, delim);
    }
    if(strlen(source_path) == 0 || strlen(target_path) == 0)
    {
        printf("Too little arguments for function restore\n");
        return;
    }

    char src_abs[PATH_MAX];
    char dest_abs[PATH_MAX];
    memset(src_abs,0,PATH_MAX);
    memset(dest_abs,0,PATH_MAX);
    errno = 0;
    if(!realpath(source_path,src_abs))
    {
        if(errno == ENOENT)
        {
            printf("'%s' doesn\'t exist", source_path);
        }
        perror("realpath");
        return;
    }
    if(!realpath(target_path,dest_abs))
    {
        if(errno == ENOENT)
        {
            printf("'%s' doesn\'t exist", target_path);
        }
        perror("realpath");
        return;
    }
    if(validate_watch(source_path,target_path,childmap))
    {
        printf("Cannot restore form an active copy\n");
        return;
    }
    if(find_child(unactive,dest_abs) == NULL)
    {
        printf("Target was never a copy - cannot restore\n");
        return;
    }
    restore_main(src_abs,dest_abs,childmap);
}
void handle_list(char* command, struct ChildMap* child_map, struct ChildMap* unactive)
{

    if(strtok(command," \n") != NULL)
    {
        printf("Command \'list\' only takes one argument\n");
        return;
    }
    printf("Active Copies: \n");
    if(child_map->children==0)
    {
        printf("No Copies Active\n");
    }
    for(int i=0; i<child_map->children;i++)
    {
        printf("Watching %s on backup %s\n", child_map->child_map->source, child_map->child_map[i].target);
    }

    printf("Deactivated Copies: \n");
    if(unactive->children==0)
    {
        printf("No Deactivated Copies\n");
    }
    for(int i=0; i<unactive->children;i++)
    {
        printf("Watching %s on backup %s\n",unactive->child_map->source, unactive->child_map[i].target);
    }
    return;
}


void usage(int argc,char* argv)
{
    fprintf(stderr,"USAGE: argc - %d, argv - %s\n", argc, argv);
    exit(EXIT_FAILURE);
}
int validate_watch(char* source_path,char* target_path,struct ChildMap* childmap)
{
    struct Child* c;
    if((c = find_child(childmap,target_path))!=NULL)
    {

        printf("Watch already exists\n");
        return -1;
    }

    if(strcmp(target_path,source_path)==0)
    {
        printf("Target name and Source name are the same\n");
        return -1;
    }

    return 0;
}

void delete_map(struct ChildMap* child_map)
{
    for(int i=0; i<child_map->children; i++)
    {
        printf("Deleting watch '%s' -> '%s'\n", child_map->child_map[i].source,child_map->child_map[i].target);
        free(child_map->child_map[i].target);
        free(child_map->child_map[i].source);

    }
}
void remove_from_child_map_pid(struct ChildMap *map, pid_t pid) {
    for (int i = 0; i < map->children; i++) {
        if (map->child_map[i].child_pid == pid) {
            printf("removing watch: '%s' -> '%s' \n",map->child_map[i].source, map->child_map[i].target);
            free(map->child_map[i].target);
            free(map->child_map[i].source);
            map->child_map[i] = map->child_map[map->children - 1];
            map->children--;
            return;
        }
    }
}

void end_main(char* src, char* dest, struct ChildMap* child_map,struct ChildMap* unactive)
{
    struct Child* c;
    if((c = find_child(child_map,dest))==NULL)
    {
        printf("There is no backup: '%s' -> '%s'\n",src,dest);
    }
    else{
        if(strcmp(c->source,src) != 0)
        {
            printf("There is no backup: '%s' -> '%s'\n",src,dest);
        }
        else{
            if(kill(c->child_pid,SIGUSR1)==-1)
            {
                perror("kill");
                return;
            }
            add_to_child_map(unactive,c->child_pid,src,dest);
            remove_from_child_map(child_map,dest);

        }
    }
}
int check_forbidden_chars(char* path)
{
    const char *forbidden_chars = "&|;`$<>(){}\"";
    for(size_t i=0; i<strlen(forbidden_chars);i++)
    {
        if(strchr(path,forbidden_chars[i])!=NULL)
        {
            return -1;
        }
    }
    return 0;
}
void restore_main(char* source, char* target,struct ChildMap* child_map)
{
    printf("Restoring...\n");
    if(kill(0,SIGUSR1)==-1)
        return;
    while(wait(NULL) > 0);
    clear_dir(source);
    process_tree(target,source,target,source);
    for(int i=0;i<child_map->children;i++)
    {
        clear_dir(child_map->child_map[i].target);
        pid_t kid_pid = fork();
        if (kid_pid == -1) {
            perror("fork");
            continue;
        }
        if (kid_pid == 0) {
            add_main(child_map->child_map[i].source, child_map->child_map[i].target);
            exit(EXIT_SUCCESS);
        }
        else {
            child_map->child_map[i].child_pid = kid_pid;
        }
    }

}
