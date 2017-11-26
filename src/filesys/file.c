#include "filesys/file.h"
#include <debug.h>
#include <math.h>
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "filesys/fdtable.h"
#include "threads/thread.h"


static void flip_bit(unsigned long *, unsigned long);


struct file*
fd_file(int _fd)
{
    struct thread * cur;
    cur = thread_current();

    ASSERT(cur->files != NULL && cur->files->fdt != NULL 
            && cur->files->fdt->fd != NULL);

    return cur->files->fdt->fd[_fd];
}

struct file_struct *
new_file_struct(void)
{
    struct file_struct * fstr;
    struct fdtable *fdt;

    fstr = malloc(sizeof(struct file_struct));
    if(fstr == NULL)
        goto end;
    fdt =  malloc(sizeof(struct fdtable));
    if(fdt == NULL)
        goto done_fstr;

    fdt->open_fds = calloc(1, sizeof(unsigned long) * FD_MAX_NR/BITS_PER_LONG);
    if(fdt->open_fds == NULL)
        goto done_fdt;

    fdt->max_fds = FD_MAX_NR;
    fdt->fd = calloc(1, sizeof(struct file *) * FD_MAX_NR);
    if(fdt->fd == NULL)
        goto done_open_fds;

    fstr->fdt = fdt;
    fstr->count = 0;


    return fstr;

done_open_fds:
   free(fdt->open_fds); 

done_fdt:
    free(fdt);

done_fstr:
    free(fstr); 
end:
    return NULL;

}


/* Frees a file_struct */
void
free_file_struct(struct file_struct * fs)
{
    ASSERT(fs != NULL && fs->fdt != NULL);

    free(fs->fdt->fd);
    free(fs->fdt->open_fds);
    free(fs->fdt);
    free(fs);

    return;
}


/* Rely on the fact that l != ~0UL
 * Return  [1, BITS_PER_LONG-1] */
static
unsigned long
ffz(unsigned long l)
{
    unsigned long i = 0;
    while(l & (1<<i++));

    return i;
}

/* Slow version of finding next_zero_bit
 * fast version: 
 * @elixir.free-electrons.com/linux/v4.14/source/lib/find_bit.c#L31
 */
static
unsigned int
next_zero_bit(unsigned long * start, unsigned long size)
{
    unsigned long i;

    for(i = 0; i * BITS_PER_LONG < size; i++) {
        if(start[i] != ~0UL)
            return min(size, BITS_PER_LONG * i + ffz(start[i]));
    }

    return size; 
}

void
flip_bit(unsigned long * open_fds, unsigned long bit)
{
    unsigned long bits;
    unsigned int start;

    start = 0;
    bit--;

    bits = open_fds[start+bit/BITS_PER_LONG];
    open_fds[start] = bits ^ (1UL << bit);
}


static void
set_bit(unsigned long * start, unsigned long bit, bool to_one)
{
    unsigned long bits, i, offset, remain;
    i = 0;
    bit--;
    
    offset = bit/BITS_PER_LONG;
    remain = bit%BITS_PER_LONG;
    bits = start[i + offset];
    start[i+offset] = (to_one ? (bits | (1UL << remain)) 
            : (bits & ~(1UL << remain)));
}


/* Find the next available fd */
static int
find_next_fd(struct file_struct * fstruc){
    struct fdtable * fdt;
    unsigned int fd;

    fdt = fstruc->fdt;
    fd = next_zero_bit(fdt->open_fds, fdt->max_fds); 
    
    if(fd == fdt->max_fds)
        return FD_ERROR; /* TODO: expanding the fdtable when 
                            hitting max */
    
    return (int)(fd+FD_OFFSET);
}


/* TODO: handle explicitly unset the bit */
void
unset_fd_bit(unsigned long * open_fds, unsigned long bit)
{
    set_bit(open_fds, bit, false);
}

static void
set_fd(int fd, struct file_struct * fstr)
{
    unsigned long fd_bit;
    ASSERT(fd > FD_OFFSET);

    fd_bit = (unsigned long) (fd - FD_OFFSET);
    
    flip_bit(fstr->fdt->open_fds, fd_bit);

}

/* Assign a file * to a fd, and marks the fd bit*/
static void
assign_fd(int fd, struct file * file, struct file_struct * fstr)
{
   struct file ** fd_arr;
   ASSERT(fstr != NULL);

   fd_arr = fstr->fdt->fd;
   fd_arr[fd] = file;

   set_fd(fd, fstr);
}

/* Allocates a fd to the opened file. 
 * TODO: opened file number is static for now (FILE_OPEN_NR) */
int
alloc_fd(struct file * file)
{
    struct thread * cur;
    int next_fd;

    cur = thread_current();
    next_fd = find_next_fd(cur->files);
    
    if(next_fd != FD_ERROR)
        assign_fd(next_fd, file, cur->files);
    return next_fd;
}


/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}

/* Opens and returns a new file for the same inode as FILE.
   Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) 
{
  return file_open (inode_reopen (file->inode));
}

/* Closes FILE. */
void
file_close (struct file *file) 
{
  if (file != NULL)
    {
      file_allow_write (file);
      inode_close (file->inode);
      free (file); 
    }
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) 
{
  return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at the file's current position.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size) 
{
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;
  return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) 
{
  return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at the file's current position.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size) 
{
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;
  return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs) 
{
  return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (!file->deny_write) 
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file) 
{
  ASSERT (file != NULL);
  if (file->deny_write) 
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}

int
file_size (int fd)
{
    struct file * file;
    file = fd_file(fd);

    if(file != NULL)
        return file_length(file);

    return 0;
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) 
{
  ASSERT (file != NULL);
  return inode_length (file->inode);
}

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file. */
void
file_seek (struct file *file, off_t new_pos)
{
  ASSERT (file != NULL);
  ASSERT (new_pos >= 0);
  file->pos = new_pos;
}

/* Returns the current position in FILE as a byte offset from the
   start of the file. */
off_t
file_tell (struct file *file) 
{
  ASSERT (file != NULL);
  return file->pos;
}
