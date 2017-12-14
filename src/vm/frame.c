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
#include "threads/vaddr.h"
#include "threads/thread.h"
#include <stdint.h>


static void * evict(void);                           /*Evict a frame*/
static void add_frame(void* faddr, void* page);
unsigned frame_hash_func(const struct hash_elem *f_hash, void *aux UNUSED);
bool frame_hash_less (const struct hash_elem *a_, const struct hash_elem *b_,
        void *aux UNUSED);





static struct hash frames_hash;
static struct list frames_list;
static struct lock frames_lock;



/* Get a frame. Should not return NULL*/
void *
vm_get_frame(enum palloc_flags flags, void * vuaddr) 
{
    void* frame,* page;

    frame = palloc_get_page(flags);
    if(frame == NULL) {
        //Evict a frame 
        frame = evict();
    }
    ASSERT(frame != NULL);

    page =  pg_round_down(vuaddr); 
    add_frame(frame, page);
    return frame;
}

void 
vm_free_frame(void * faddr) 
{
    
    struct frame * f, _f;
    struct hash_elem * he;
    _f.faddr = faddr;

    he = hash_find(&frames_hash, &_f.helem);
    if(he != NULL) {
        f = hash_entry(he, struct frame, helem);
        hash_delete(&frames_hash, &f->helem);
        free(f);
    }
}


static void
add_frame(void * frame, void* page) 
{
    struct frame* f;
    struct hash_elem * he;

    lock_acquire(&frames_lock);

    f = malloc(sizeof(struct frame));
    if(f == NULL) {
        lock_release(&frames_lock);
        ASSERT(false); // TODO
    }

    f->faddr = frame;
    f->upage = page;
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
evict(void)
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
frame_hash_func(const struct hash_elem *f_hash, void *aux UNUSED) 
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

