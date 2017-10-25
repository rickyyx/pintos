#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"


/* Syscalls */
static void syscall_handler (struct intr_frame *);
static void syscall_exit(int*, struct intr_frame*);

/* Utility methods */
static bool valid_syscall_num(const int);
static int get_syscall_argv(int, int*, int*);
static bool valid_user_vaddr(void*);

static void _exit(const int);

typedef void (*syscall_func) (int*, struct intr_frame*);
static syscall_func syscall_table[SYS_NUM];
static int syscall_argc_table[SYS_NUM];



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  //sys_exit
  syscall_table[SYS_EXIT] = syscall_exit;
  syscall_argc_table[SYS_EXIT] = 1;
  printf("System number of exit : %d\n", SYS_EXIT);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num, * argv;
  int * esp = f->esp;

  if(!is_user_vaddr(esp) && valid_syscall_num(*esp)){
      _exit(-1);
  }

  syscall_num = *esp++;
  argv = malloc(4 * sizeof(int));

  if(!get_syscall_argv(syscall_num, argv, esp)) {
      free(argv);
      _exit(-1);
  }

  syscall_table[syscall_num](argv, f);
}


static inline bool
valid_syscall_num(const int num) 
{
    return SYS_HALT <= num && num <SYS_INUMBER;
}


static int
get_syscall_argv(const int syscall_num, int * argv, int* esp) 
{
    int syscall_argc = syscall_argc_table[syscall_num];

    while(syscall_argc--){
        if(!valid_user_vaddr(esp))
            return -1;
        *argv++ = *esp++;
    }
    return 0;
}


static bool 
valid_user_vaddr(void * addr)
{
    //TODO: Check if the page is mapped 
    return addr != NULL && is_user_vaddr(addr);
}

static void
_exit(int status)
{
    struct thread * cur = thread_current();

    printf ("%s: exit(%d)\n",cur->name, status);
    thread_exit();
}

static void
syscall_exit(int* argv, struct intr_frame * cf UNUSED)
{
    _exit(*argv);
}


