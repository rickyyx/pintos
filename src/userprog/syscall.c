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
#include "filesys/file.h"
#include "filesys/fdtable.h"
#include "filesys/off_t.h"
#include "filesys/inode.h"
#include "devices/shutdown.h"
#include "devices/input.h"
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
static void syscall_remove(int*, struct intr_frame*);
static void syscall_close(int*, struct intr_frame*);
static void syscall_filesize(int*, struct intr_frame *);
static void syscall_read(int*, struct intr_frame *);
static void syscall_seek(int*, struct intr_frame *);
static void syscall_tell(int*, struct intr_frame*);


/* Utility methods */
static bool valid_syscall_num(const int);
static bool get_syscall_argv(int,  int*);
static bool valid_user_vaddr(const void*);
static bool valid_fd(int);

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

  //read
  syscall_table[SYS_READ] = syscall_read;
  syscall_argc_table[SYS_READ] = 3;

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

  //close
  syscall_table[SYS_CLOSE] = syscall_close;
  syscall_argc_table[SYS_CLOSE] = 1;

  //filesize
  syscall_table[SYS_FILESIZE] = syscall_filesize;
  syscall_argc_table[SYS_FILESIZE] = 1;

  //seek
  syscall_table[SYS_SEEK] = syscall_seek;
  syscall_argc_table[SYS_SEEK] = 2;

  //tell
  syscall_table[SYS_TELL] = syscall_tell;
  syscall_argc_table[SYS_TELL] = 1;

  //remove
  syscall_table[SYS_REMOVE] = syscall_remove;
  syscall_argc_table[SYS_REMOVE] = 1;

}

static void
syscall_tell(int* argv, struct intr_frame * f)
{
    int fd = *(int*) argv++;
    struct file * file;
    off_t offset;

    if(!valid_fd(fd))
        _exit(-1);

    file = fd_file(fd);
    if(file == NULL) {
        offset = 0;
        goto end;
    }

    lock_acquire(&sys_filesys_lock);
    offset = file_tell(file);
    lock_release(&sys_filesys_lock);

end:
    f->eax = (uint32_t) offset;
}

static void
syscall_seek(int* argv, struct intr_frame* f UNUSED)
{
    int fd = *(int*) argv++;
    unsigned pos = *(unsigned *) argv;

    struct file * file;
    
    if(!valid_fd(fd))
        _exit(-1);

    file = fd_file(fd);

    if(file == NULL)
        return;

    lock_acquire(&sys_filesys_lock);
    file_seek(file, (off_t) pos);    
    lock_release(&sys_filesys_lock);
}


static void
syscall_filesize(int * argv, struct intr_frame*f)
{
    int fd = *(int*) argv;

    if(fd - FD_OFFSET < FD_MAX_NR && fd -FD_OFFSET >=0) {
        lock_acquire(&sys_filesys_lock);
        f->eax =(uint32_t)file_size(fd);
        lock_release(&sys_filesys_lock);
    } else {
        f->eax = (uint32_t) 0;
    }
}

static void
syscall_read(int * argv, struct intr_frame * f)
{
    int fd = *(int*) argv++;
    void * buffer = * (void**) argv++;
    unsigned size = *(unsigned *) argv, i;
    struct file * file;
    uint8_t* buf_stdin;
    off_t ret = -1;
    
    
    if((!valid_user_vaddr(buffer))
            || (!valid_fd(fd) && fd != STDIN_FILENO))
        _exit(-1);

    if(fd == STDIN_FILENO){
        ret = 0;
        i = 0;
        buf_stdin = (uint8_t*) buffer;
        while(i < size) {
            *buf_stdin++ = input_getc();    
            ret++; i++;
        }
    } else {
        lock_acquire(&sys_filesys_lock);

        file = fd_file(fd);
        if(file != NULL){
            ret = file_read(file, buffer, (off_t) size);
        }
        lock_release(&sys_filesys_lock);
    }

    f->eax = (uint32_t) ret;
}

static void
syscall_remove(int * argv, struct intr_frame *f)
{

    const char * file = *(char**) argv;
    if(!valid_user_vaddr(file))
        _exit(-1);

    lock_acquire(&sys_filesys_lock);
    f->eax = filesys_remove(file);
    lock_release(&sys_filesys_lock);
}

static void
syscall_close(int * argv, struct intr_frame *f UNUSED)
{
    int fd = *(int*) argv;
    
    if(valid_fd(fd)) {
        lock_acquire(&sys_filesys_lock);
        process_close(fd);
        lock_release(&sys_filesys_lock);
    }
}


static void
syscall_open(int * argv, struct intr_frame *f )
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
  int syscall_num;
  int * esp = f->esp;

  if(!valid_user_vaddr(esp) || !valid_syscall_num(*esp)){
      _exit(-1);
  }

  syscall_num = *esp++;

  if(!get_syscall_argv(syscall_num, esp)) {
      _exit(-1);
  }

  syscall_table[syscall_num](esp, f);
}


static inline bool
valid_syscall_num(const int num) 
{
    return SYS_HALT <= num && num <SYS_INUMBER;
}


static bool
get_syscall_argv(const int syscall_num, int* esp) 
{
    int syscall_argc = syscall_argc_table[syscall_num];

    while(syscall_argc--){
        if(!valid_user_vaddr(esp))
            return false;
        esp++;
    }
    return true;
}


static bool 
valid_user_vaddr(const void * addr)
{
    return (addr != NULL && is_user_vaddr(addr) 
            && lookup_page(thread_current()->pagedir, addr, false) != NULL);
}

static void
_exit(int status)
{
    struct thread * cur = thread_current();
    cur->exit_status = status;
    thread_exit();
}

static void
syscall_write(int* argv, struct intr_frame * cf)
{
    int fd = *(int*)argv++;
    void * buffer = *(void**) argv++;
    unsigned size = *(unsigned*)argv++;
    struct file * file;
    
    if(!(valid_fd(fd) || fd == STDOUT_FILENO || fd == STDERR_FILENO)
            || !valid_user_vaddr(buffer)) {
       _exit(-1); 
    }

    if(fd == STDOUT_FILENO)
        putbuf(buffer, size);
    else {
        if((file=fd_file(fd)) == NULL)
            _exit(-1);

        if(!inode_can_write(file->inode)) { 
            size = 0;
            goto end;
        }

        lock_acquire(&sys_filesys_lock);
        size = file_write(file, buffer, (off_t) size);
        lock_release(&sys_filesys_lock);
    }
end: 
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

static bool
valid_fd(int fd)
{
    if(fd - FD_OFFSET < FD_MAX_NR && fd -FD_OFFSET >=0) {
        return true;
    } else 
        return false;
}
