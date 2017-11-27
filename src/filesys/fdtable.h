#ifndef FILESYS_FDTABLE_H
#define FILESYS_FDTABLE_H

#include "filesys/off_t.h"

struct file_struct {
    struct fdtable * fdt;       /* pointer to fd table */
    int count;
};

#define FD_ERROR -1
#define FD_OFFSET 2
#define FD_MAX_NR 256

#define BITS_PER_LONG 32
struct fdtable {
    unsigned int max_fds;
    //unsigned int next_fd;

    struct file ** fd;

    unsigned long * open_fds;
};


struct file* fd_file(int);
off_t file_size(int fd);

int alloc_fd(struct file *);
struct file_struct* new_file_struct(void);
void free_file_struct(struct file_struct*);

void unset_fd_bit(struct fdtable*, unsigned long);



#endif /* filesys/fdtable.h */
