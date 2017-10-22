#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct cmd_frame 
{
    char * prog_name;                   /* Program name */
    char * prog_args;                   /* Program arguments after name*/
    int argc;                           /* Argument counts */
    char * boundary;                    /* Boundary addrss of the arguments */
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
