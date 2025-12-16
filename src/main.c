#include "main.h"
#include "processing_commands.h"
#include "copying.h"
#include "add.h"
void ignore_all_signals_except_critical() {
    struct sigaction sa;
    memset(&sa,0,sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN;

    for (int i = 1; i < NSIG; i++) {

        if (i == SIGINT || i == SIGTERM) {
            continue;
        }

        if (i == SIGKILL || i == SIGSTOP) {
            continue;
        }

        if (sigaction(i, &sa, NULL) == -1) {
        }
    }
}

int main(int argc,char** argv)
{
    if(argc!=1)
        usage(argc,argv[0]);
    printf("------------------------------------------------\n");
    printf("Welcome to Backup Manager! Here are your options\n");
    printf(" - add <source path> <target path>\n");
    printf(" - end <source path> <target paths>\n");
    printf(" - list\n");
    printf(" - restore <source path> <target path>\n");
    printf(" - exit\n");
    printf("------------------------------------------------\n");
//    char* source = "/home/ksobczynski/SOP1/p1/Source_dir";
//    char* target = "/home/ksobczynski/SOP1/p1/Target_dir";
//    add_main(source,target);
//    while(1)
//    {

//    }
    ignore_all_signals_except_critical();
    int end_time = 0;
    struct ChildMap child_map = {0};
    struct ChildMap unactive = {0};
    while(end_time == 0) {
        char command[PATH_MAX];
        if(fgets(command,PATH_MAX,stdin) == NULL) // TODO zmien
        {
            perror("fgets");
            continue;
        }
        handle_command(command,&child_map,&unactive,&end_time);
        pid_t kid_pid;
        while ((kid_pid = waitpid(-1,NULL,WNOHANG))>0)
        {
            remove_from_child_map_pid(&child_map,kid_pid);
        }
    }
    if(kill(0,SIGUSR1)==-1)
    {
        perror("kill");
    }
    printf("Exiting program...\n");
    delete_map(&child_map);
    delete_map(&unactive);
    while (wait(NULL)>0);
}

