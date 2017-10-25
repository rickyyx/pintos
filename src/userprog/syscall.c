#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"


/* Syscalls */
static void syscall_handler (struct intr_frame *);
static void _exit(const int);


/* Utility methods */
static bool valid_syscall_num(const int);
static int get_syscall_argv(int, int*);


typedef void (*syscall_func) (int*, struct intr_frame*);
static syscall_func syscall_table[SYS_NUM];



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num, * argv;
  void * esp = f->esp;

  if(!is_user_vaddr(esp) && valid_syscall_num(*(int*)esp)){
      _exit(-1);
  }

  syscall_num = *(int*)esp;
  argv = malloc(4 * sizeof(int));

  if(!get_syscall_argv(syscall_num, argv)) {
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
get_syscall_argv(const int syscall_num UNUSED, int * argv UNUSED) 
{
    return 1;
}

static void
_exit(int status UNUSED)
{

}


