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

struct list frames;                             /* Frame lists*/

struct frame {
    void * faddr;                               /* Frame address*/
    void * page;                                /* Page address*/
    struct list_elem elem;                      /* List element for the frames list*/
};

void * get_frame(void * page, enum palloc_flags);                             /* Get a new frame */


#endif /* vm/frame.h */

