/*
 * =====================================================================================
 *
 *       Filename:  frame.h
 *
 *    Description:  Header for frame managements
 *
 *        Version:  1.0
 *        Created:  12/02/17 16:58:11
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ricky (), 
 *   Organization:  
 *
 * =====================================================================================
 */


#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include <list.h>
#include <hash.h>

struct list frames;                             /* Frame lists*/

struct frame {
    void * faddr;                               /* Frame address*/
    void * upage;                               /* User Page address*/
    struct list_elem lelem;                      /* List element for the frames list*/
    struct hash_elem helem;                     /* Hash element for the frames table */
    struct thread * t;                          /* Owned by which process */
};

void vm_frame_init(void);                           /* Init the frame table */
void * vm_get_frame(enum palloc_flags, void* uaddr);   /* Get a new frame */
void vm_free_frame(void * upage);            /* Frees a user page */


#endif /* vm/frame.h */

