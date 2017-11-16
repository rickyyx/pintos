#ifndef FILESYS_FDTABLE_H
#define FILESYS_FDTABLE_H

struct file_struct {
    struct fdtable * fdt;       /* pointer to fd table */
    int count;
};

#define FD_ERROR -1
#define FD_OFFSET 2

struct fdtable {
    unsigned int max_fds;
    unsigned int next_fd;

    struct file ** fd;

    unsigned long * open_fds;
};

int alloc_fd(struct file *);



#endif /* filesys/fdtable.h */
