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
#include "threads/synch.h"


static void * evict();                           /*Evict a frame*/
static void add_frame(void* faddr, void* page);


static struct hash frames_hash;
static struct list frames_list;
static struct lock frames_lock;



/* Get a frame. Should not return NULL*/
void *
vm_get_frame(void * vuaddr, enum palloc_flags) 
{
    void* frame;
    uintptr_t page;

    frame = palloc_get_page(palloc_flags);
    if(frame == NULL) {
        //Evict a frame 
        faddr = evict();
    }
    ASSERT(faddr != NULL);

    page = (uintptr_t) pg_round_down(vuaddr); 
    add_frame(frame, page);
    return frame;
}


static void
add_frame(void * frame, uintptr_t page) 
{
    struct frame* f_struct;
    struct hash_elem * he;

    lock_acquire(&frames_lock);

    f = malloc(sizeof(struct frame));
    if(f == NULL) {
        lock_release(&frames_lock);
        ASSERT(); // TODO
    }

    f->faddr = faddr;
    f->page = page;
    f->t = thread_current();

    list_push_back(&frames_list, &f->lelem);
    he = hash_insert(&frames_hash, &f->helem);
    ASSERT(he == NULL); /* The frame should be free before calling this method */
    
    lock_release(&frames_lock);
    return;
}


// TODO
/*  Evicts a frame using randome algorithm */
static void *
evict()
{
   return NULL; 
}


void
vm_frame_init(void) 
{
    hash_init(&frames_hash, frame_hash_func, frame_hash_less, NULL);
    list_init(&frames_list);
    lock_init(&frames_lock);
}


unsigned 
frame_hash_func(cost struct hash_elem *f_hash, void *aux UNUSED) 
{
    const struct frame *f = hash_entry(f_hash,struct frame, helem);
    return hash_bytes(&f->faddr, sizeof f->faddr);
}

bool
frame_hash_less (const struct hash_elem *a_, const struct hash_elem *b_,
        void *aux UNUSED)
{
    const struct frame *a = hash_entry (a_, struct frame, helem);
    const struct frame *b = hash_entry (b_, struct frame, helem);
    return a->faddr < b->faddr;
}

