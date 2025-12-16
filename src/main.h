#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>





int main(int argc,char** argv);
void usage(int argc, char* argv);
void ignore_all_signals_except_critical();






