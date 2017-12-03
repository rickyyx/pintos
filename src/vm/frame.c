/*
 * =====================================================================================
 *
 *       Filename:  frame.c
 *
 *    Description:  Implementation for frames
 *
 *        Version:  1.0
 *        Created:  12/02/17 17:31:59
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ricky (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "vm/frame.h"
#include "threads/malloc.h"

static void * evict();                           /*Evict a frame*/
static void add_frame(void* faddr, void* page);



/* Get a frame. Always returns a valid frame pointer */
void *
get_frame(void * page, enum palloc_flags) 
{
    void* faddr;

    faddr = palloc_get_page(PAL_USER | palloc_flags);
    if(frame == NULL) {
        faddr = evict();
    }
    
    ASSERT(faddr != NULL);

    add_frame(faddr, page);
    return faddr;
}


static void
add_frame(void * faddr, void * page) 
{
    struct frame* f;

    f = malloc(sizeof(struct frame));
    if(f == NULL) {
        ASSERT(); // TODO
    }

    f->faddr = faddr;
    f->page = page;

    list_push_back(&frames, &f->elem);

    return;
}


// TODO
/*  Evicts a frame using randome algorithm */
static void *
evict()
{
   return NULL; 
}


