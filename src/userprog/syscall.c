#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"
#include "devices/shutdown.h"
#include <user/syscall.h>

/* Syscalls */
static void syscall_handler (struct intr_frame *);
static void syscall_exit(int*, struct intr_frame*);
static void syscall_write(int*, struct intr_frame*);
static void syscall_halt(int*, struct intr_frame*);
static void syscall_exec(int*, struct intr_frame*);
static void syscall_wait(int*, struct intr_frame*);
static void syscall_create(int*, struct intr_frame*);
static void syscall_open(int*, struct intr_frame*);

/* Utility methods */
static bool valid_syscall_num(const int);
static bool get_syscall_argv(int, int*, int*);
static bool valid_user_vaddr(const void*);

static void _exit(const int);

typedef void (*syscall_func) (int*, struct intr_frame*);
static syscall_func syscall_table[SYS_NUM];
static int syscall_argc_table[SYS_NUM];
static struct lock sys_filesys_lock;


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init(&sys_filesys_lock);


  //sys_exit
  syscall_table[SYS_EXIT] = syscall_exit;
  syscall_argc_table[SYS_EXIT] = 1;

  //write
  syscall_table[SYS_WRITE] = syscall_write;
  syscall_argc_table[SYS_WRITE] = 3;

  //halt
  syscall_table[SYS_HALT] = syscall_halt;
  syscall_argc_table[SYS_HALT] = 0;

  //exec
  syscall_table[SYS_EXEC] = syscall_exec;
  syscall_argc_table[SYS_EXEC] = 1;

  //wait
  syscall_table[SYS_WAIT] = syscall_wait;
  syscall_argc_table[SYS_WAIT] = 1;

  //create
  syscall_table[SYS_CREATE] = syscall_create;
  syscall_argc_table[SYS_CREATE] = 2;
    
  //open
  syscall_table[SYS_OPEN] = syscall_open;
  syscall_argc_table[SYS_OPEN] = 1;
}


static void
syscall_open(int * arg, struct intr_frame *f )
{
    const char * file = *(char**) argv;
    int fd;
    if(!valid_user_vaddr(file))
        _exit(-1);

    lock_acquire(&sys_filesys_lock);
    fd = process_open(file);
    lock_release(&sys_filesys_lock);

    ASSERT(fd != STDIN_FILENO && fd != STDOUT_FILENO);

    f->eax = (uint32_t) fd;
}

static void
syscall_create(int* argv, struct intr_frame *f)
{
    const char *file = *(char**) argv++;
    unsigned initial_size = *(unsigned *) argv;
    bool success = false;

    if(!valid_user_vaddr(file))
        _exit(-1);
    
    lock_acquire(&sys_filesys_lock);
    success  = filesys_create(file,(off_t)initial_size);
    lock_release(&sys_filesys_lock);

    f->eax =(uint32_t) success;
}


static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num, * argv;
  int * esp = f->esp;

  if(!valid_user_vaddr(esp) || !valid_syscall_num(*esp)){
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


static bool
get_syscall_argv(const int syscall_num, int * argv, int* esp) 
{
    int syscall_argc = syscall_argc_table[syscall_num];

    while(syscall_argc--){
        if(!valid_user_vaddr(esp))
            return false;
        *argv++ = *esp++;
    }
    return true;
}


static bool 
valid_user_vaddr(const void * addr)
{
    //TODO: Check if the page is mapped 
    
    return (addr != NULL && is_user_vaddr(addr) 
            && lookup_page(thread_current()->pagedir, addr, false) != NULL);
}

static void
_exit(int status)
{
    struct thread * cur = thread_current();
    printf ("%s: exit(%d)\n",cur->name, status);
    cur->exit_status = status;
    thread_exit();
}

static void
syscall_write(int* argv, struct intr_frame * cf)
{
    int fd = *(int*)argv++;
    void * buffer = *(void**) argv++;
    unsigned size = *(unsigned*)argv++;
    
    if(fd == 1) {
        putbuf(buffer, size);
    }

    //TODO: fd != 1?
    cf->eax = (uint32_t) size;
}


static void
syscall_exit(int* argv, struct intr_frame * cf UNUSED)
{
    _exit(*argv);
}

static void
syscall_halt(int* argv UNUSED, struct intr_frame * cf UNUSED)
{
    shutdown_power_off();
}

static void
syscall_exec(int* argv, struct intr_frame * cf)
{
    const char *cmd_line = *(char**) argv;
    pid_t pid = (pid_t) TID_ERROR;
    if(valid_user_vaddr(cmd_line))
        pid = process_execute(cmd_line);

    cf->eax = (uint32_t) pid;
}

static void
syscall_wait(int* argv, struct intr_frame * cf)
{
    pid_t pid = *(pid_t *)argv;
    cf->eax = process_wait((tid_t)pid);
}

