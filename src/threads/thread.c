#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "lib/fpoint.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of processes in THREAD_SLEEP state */
static struct list sleep_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
{
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

int32_t load_avg;
int ready_threads_cnt;

#define MLFQS_RQ_SIZE PRI_MAX-PRI_MIN+1
static struct list rq[MLFQS_RQ_SIZE];

static void init_rq(void);
static void thread_update_rq(struct thread *);
static bool thread_mlfqs_highest_priority(void);
static void thread_mlfqs_deque(struct thread *);
static void thread_mlfqs_enque(struct thread *);
static int mlfqs_highest_priority(void);
static void update_cur_recent_cpu(void);
static void update_thread_recent_cpu(struct thread *, void*);
static void update_all_recent_cpu(void);
static void update_load_avg(void);
static void update_all_priority(void);
static void update_thread_priority(struct thread*, void *);
static int count_ready_threads(void);
static int calculate_priority(struct thread *);

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int, int, int32_t);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static bool should_wakeup(struct list_elem *, int64_t);
static void schedule (void);
static void wake_threads_up(void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);


/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
    void
thread_init (void) 
{
    ASSERT (intr_get_level () == INTR_OFF);

    lock_init (&tid_lock);
    if(!thread_mlfqs)
        list_init (&ready_list);
    else 
        init_rq();
    list_init (&all_list);
    list_init (&sleep_list);
    
    /* Init load_avg at begininng */
    load_avg = F_TOFPOINT(0);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread ();

    /* Init the main thread with default values for scheduling */
    init_thread (initial_thread, "main", PRI_DEFAULT, NICE_DEFAULT, F_TOFPOINT(0));
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid ();
    initial_thread->wake_up_time = 0;

}

/* Initializes the run queue for MLFQS scheduling */
void
init_rq(void)
{
    int i;
    for(i = 0; i<MLFQS_RQ_SIZE; i++) {
        list_init(&rq[i]);
    }
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
    void
thread_start (void) 
{
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init (&idle_started, 0);
    thread_create ("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable ();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
    void
thread_tick (void) 
{
    struct thread *t = thread_current ();

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
        user_ticks++;
#endif
    else
        kernel_ticks++;

    /* Update cur running thread's recent cpu */
    update_cur_recent_cpu();

    /* Update load_av, all threads recent_cpu / SECOND */
    if(timer_ticks() % TIMER_FREQ == 0 ){
        update_load_avg();
        update_all_recent_cpu();
    }

    /* Update piority / 4th tick */
    if(timer_ticks() % 4 == 0)
        update_all_priority();

    /* Yield on running out of time_slice / low priority */
    if (++thread_ticks >= TIME_SLICE || !thread_mlfqs_highest_priority())
        intr_yield_on_return ();

    /* Update the ready list by dequeing from sleep_list */
    wake_threads_up(); 
}


static void
update_cur_recent_cpu(void)
{
    struct thread* t = thread_current();

    if( t == idle_thread) return;

    t->recent_cpu++;
    t->recent_cpu_dirty = true;
}

static void 
update_all_recent_cpu(void)
{
   thread_foreach(update_thread_recent_cpu, NULL); 
}

static void
update_thread_recent_cpu(struct thread *t, void *aux UNUSED)
{
    ASSERT(is_thread(t));

    if(t == idle_thread) return;

    int32_t cpu = t->recent_cpu;
    t->recent_cpu = F_ADD_INT(F_MULTIPLE(F_DIVIDE(cpu*2, F_ADD_INT(cpu*2, 1)), cpu), t->nice);
    if(t->recent_cpu != cpu)
        t->recent_cpu_dirty = true;
}

static void
update_all_priority(void)
{
    thread_foreach(update_thread_priority, NULL);
}

static void
update_thread_priority(struct thread *t, void *aux UNUSED)
{
    if(!t->recent_cpu_dirty) return;

    int old_pri = t->priority;
    t->priority = calculate_priority(t);
    ASSERT(t->priority <= PRI_MAX && t->priority >= PRI_MIN);

    if(old_pri != t->priority){
        thread_update_rq(t);
    }
}

static int
calculate_priority(struct thread *t)
{
    int pri = PRI_MAX-(t->nice *2) - F_TOINT_NEAR(F_DIVIDE_INT(t->recent_cpu, 4));
    if(pri > PRI_MAX) return PRI_MAX;
    if(pri < PRI_MIN) return PRI_MIN;

    return pri;
}

static void
update_load_avg(void)
{
    load_avg = F_DIVIDE_INT(F_MULTIPLE_INT(load_avg, 59), 60) + F_DIVIDE_INT(F_TOFPOINT(count_ready_threads()), 60);
}

static int
count_ready_threads(void)
{
    return ready_threads_cnt;
}

/* Prints thread statistics. */
    void
thread_print_stats (void) 
{
    printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
            idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
    tid_t
thread_create (const char *name, int priority,
        thread_func *function, void *aux) 
{
    struct thread *t, *cur;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT (function != NULL);

    /* Allocate thread. */
    t = palloc_get_page (PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    cur = thread_current();
    init_thread (t, name, priority, cur->nice,cur->recent_cpu);
    tid = t->tid = allocate_tid ();

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame (t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame (t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame (t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    /* Add to run queue. */
    thread_unblock (t);

    /* Schedule with higher priority thread */
    //TODO
    if(thread_current()->priority < priority) {
        thread_yield();        
    }

    return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
    void
thread_block (void) 
{
    ASSERT (!intr_context ());
    ASSERT (intr_get_level () == INTR_OFF);

    thread_current ()->status = THREAD_BLOCKED;
    schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
    void
thread_unblock (struct thread *t) 
{
    enum intr_level old_level;

    ASSERT (is_thread (t));

    old_level = intr_disable ();
    ASSERT (t->status == THREAD_BLOCKED);
    if(!thread_mlfqs) 
        list_insert_ordered(&ready_list, &t->elem, thread_less_priority, NULL);
    else
        thread_mlfqs_enque(t);
    t->waiting_lock = NULL;
    t->status = THREAD_READY;
    intr_set_level (old_level);
}

/* Returns the name of the running thread. */
    const char *
thread_name (void) 
{
    return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
    struct thread *
thread_current (void) 
{
    struct thread *t = running_thread ();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT (is_thread (t));
    ASSERT (t->status == THREAD_RUNNING);

    return t;
}

/* Returns the running thread's tid. */
    tid_t
thread_tid (void) 
{
    return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
    void
thread_exit (void) 
{
    ASSERT (!intr_context ());

#ifdef USERPROG
    process_exit ();
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable ();
    list_remove (&thread_current()->allelem);
    thread_current ()->status = THREAD_DYING;
    schedule ();
    NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
    void
thread_yield (void) 
{
    struct thread *cur = thread_current ();
    enum intr_level old_level;

    ASSERT (!intr_context ());

    old_level = intr_disable ();
    if (cur != idle_thread) 
        list_insert_ordered(&ready_list, &cur->elem, thread_less_priority, NULL);
    cur->status = THREAD_READY;
    schedule ();
    intr_set_level (old_level);
}

    static bool 
wakeup_early (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
    const struct thread *a = list_entry(a_, struct thread, sleep_elem);
    const struct thread *b = list_entry(b_, struct thread, sleep_elem);

    return a->wake_up_time < b->wake_up_time;
}


/* Returns true when a has greater priority, meaning a will be placed before b
 * for ready_list
 */
 bool
thread_less_priority(const struct list_elem* a_, const struct list_elem *b_, void * list_name)
{

    const struct thread *a;
    const struct thread *b;

    a = list_entry(a_, struct thread, elem);
    b = list_entry(b_, struct thread, elem); 

    if(list_name != NULL){
        switch(*(enum list_type*) list_name){
            case DONOR:
                a = list_entry(a_, struct thread, donor_elem);
                b = list_entry(b_, struct thread, donor_elem);         
                return a->priority < b->priority;
            case WAITER:
                a = list_entry(a_, struct thread, waiter_elem);
                b = list_entry(b_, struct thread, waiter_elem); 
                return a->priority <= b->priority;
            default:
                break;
        }
    }

    return a->priority <= b->priority;
}


/* Thread Sleeps for a {ticks} number of CPU ticks. */
    void
thread_sleep(int64_t ticks)
{
    struct thread * cur = thread_current();
    enum intr_level old_level = intr_disable();

    cur->wake_up_time = ticks + timer_ticks();
    list_insert_ordered(&sleep_list,&cur->sleep_elem, wakeup_early, NULL);
    thread_block();
    intr_set_level(old_level);

}


/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
    void
thread_foreach (thread_action_func *func, void *aux)
{
    struct list_elem *e;

    ASSERT (intr_get_level () == INTR_OFF);

    for (e = list_begin (&all_list); e != list_end (&all_list);
            e = list_next (e))
    {
        struct thread *t = list_entry (e, struct thread, allelem);
        func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY.
 * However, it cannot reset the donation. 
 * */
    void
thread_set_priority (int new_priority) 
{
    struct thread * cur = thread_current();
    
    //MLFQS enabled, ignore set priority
    if(thread_mlfqs) return;

    //Set the static priority
    cur->static_priority = new_priority;

    //Determine its effective priority
    cur->priority = thread_get_priority(); 


    //Yield if no longer highest priority
    if(!thread_has_highest_priority()){
        thread_yield();
    }
}

/* Returns if the current thread has higher priority than all
 * ready therads */
    bool 
thread_has_highest_priority()
{
    if(list_empty(&ready_list)) {
        return true;
    } else {
        return thread_current()->priority >= list_entry(list_back(&ready_list), struct thread, elem)->priority;
    }
}

/* Returns the current thread's priority (Taking into account donation) . */
    int
thread_get_priority (void) 
{
    struct thread * cur  = thread_current();
    struct list_elem* max_elem;
    struct thread* max_donor;
    enum list_type type = DONOR;
    
    if(thread_mlfqs)
        return cur->priority;

    if(list_empty(&cur->donors)){
        return cur->static_priority;
    } else {
        max_elem = list_max(&cur->donors, thread_less_priority, &type);
        max_donor = list_entry(max_elem, struct thread, donor_elem);
        return cur->static_priority > max_donor->priority ? cur->static_priority : max_donor->priority;
    }
}


/* P1: Remove thread t from the ready list */
    struct thread*
thread_dequeue_ready_list(struct thread *t) 
{
    struct thread *next = list_entry(list_remove(&t->elem), struct thread, elem);
    return next;
}


/* P1: Add thread t to the ready list with ordered */
    void 
thread_queue_ready_list(struct thread *t)
{
    list_insert_ordered(&ready_list, &t->elem, thread_less_priority, NULL);
}


/* Sets the current thread's nice value to NICE. */
    void
thread_set_nice (int nice) 
{
  struct thread * cur = thread_current(); 
   cur->nice = nice;
   update_thread_priority(cur, NULL); 

   if(!thread_mlfqs_highest_priority())
       thread_yield();
}


static void
thread_update_rq(struct thread * t)
{
   thread_mlfqs_deque(t);
   thread_mlfqs_enque(t);
}

static void
thread_mlfqs_deque(struct thread *t)
{
    ASSERT(is_thread(t));
    ASSERT(t->priority <= PRI_MAX && t->priority >= PRI_MIN);

    list_remove(&t->elem);
}

static void
thread_mlfqs_enque(struct thread *t)
{
    ASSERT(is_thread(t));
    ASSERT(t->priority <= PRI_MAX && t->priority >= PRI_MIN);

    struct list q = rq[t->priority];
    list_push_back(&q, &t->elem);
}

static bool
thread_mlfqs_highest_priority(void)
{
    return false;
}

/* Returns the current thread's nice value. */
    int
thread_get_nice (void) 
{
    return thread_current()->nice;
}

/* Returns 100 times the system load average. */
    int
thread_get_load_avg (void) 
{
    /* Not yet implemented. */
    return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
    int
thread_get_recent_cpu (void) 
{
    /* Not yet implemented. */
    return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
    static void
idle (void *idle_started_ UNUSED) 
{
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current ();
    sema_up (idle_started);

    for (;;) 
    {
        /* Let someone else run. */
        intr_disable ();
        thread_block ();

        /* Re-enable interrupts and wait for the next one.

           The `sti' instruction disables interrupts until the
           completion of the next instruction, so these two
           instructions are executed atomically.  This atomicity is
           important; otherwise, an interrupt could be handled
           between re-enabling interrupts and waiting for the next
           one to occur, wasting as much as one clock tick worth of
           time.

           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
    static void
kernel_thread (thread_func *function, void *aux) 
{
    ASSERT (function != NULL);

    intr_enable ();       /* The scheduler runs with interrupts off. */
    function (aux);       /* Execute the thread function. */
    thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
    struct thread *
running_thread (void) 
{
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
    static bool
is_thread (struct thread *t)
{
    return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
    static void
init_thread (struct thread *t, const char *name, int priority, int nice, int32_t recent_cpu)
{
    enum intr_level old_level;

    ASSERT (t != NULL);
    ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT (name != NULL);

    memset (t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy (t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->magic = THREAD_MAGIC;
    t->waiting_lock = NULL;

    t->nice = nice;
    t->recent_cpu = recent_cpu;

    if(thread_mlfqs)
        priority = calculate_priority(t);
    t->priority = priority;
    t->static_priority = priority;

    list_init(&t->donors);

    old_level = intr_disable ();
    list_push_back (&all_list, &t->allelem);
    intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
    static void *
alloc_frame (struct thread *t, size_t size) 
{
    /* Stack data is always allocated in word-size units. */
    ASSERT (is_thread (t));
    ASSERT (size % sizeof (uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
    static struct thread *
next_thread_to_run (void) 
{
    if(!thread_mlfqs){
        if (list_empty (&ready_list))
            return idle_thread;
        else
            return list_entry (list_pop_back (&ready_list), struct thread, elem);
    } else {
        int runnable_pri = mlfqs_highest_priority();
        if(runnable_pri < PRI_MIN)
            return idle_thread;
        else 
            return list_entry(list_pop_front(&rq[runnable_pri]), struct thread, elem);
    }
}

/* Returns the highest priority of the runnable threads. Return PRI_MIN-1 
 * if there is none 
 * Interrputs must be off
 */
static int 
mlfqs_highest_priority(void){
    int i;
    ASSERT (intr_get_level () == INTR_OFF);

    for(i = PRI_MAX; i>= PRI_MIN; i--){
        if(!list_empty(&rq[i]))
            return i;
    }

    return i;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
    void
thread_schedule_tail (struct thread *prev)
{
    struct thread *cur = running_thread ();

    ASSERT (intr_get_level () == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate ();
#endif

    /* If the thread we switched from is dying, destroy its struct
       thread.  This must happen late so that thread_exit() doesn't
       pull out the rug under itself.  (We don't free
       initial_thread because its memory was not obtained via
       palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
        ASSERT (prev != cur);
        palloc_free_page (prev);
    }
}

/* Move threads in the sleep_list to ready_list */
    static void
wake_threads_up(void) 
{
    struct list_elem *e;
    struct thread * t; 
    int64_t cur_tick = timer_ticks();

    while(should_wakeup(list_begin(&sleep_list), cur_tick))
    {
        e = list_pop_front(&sleep_list);
        t = list_entry(e, struct thread, sleep_elem);

        thread_unblock(t);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
    static void
schedule (void) 
{
    struct thread *cur = running_thread ();
    struct thread *next = next_thread_to_run ();
    struct thread *prev = NULL;

    ASSERT (intr_get_level () == INTR_OFF);
    ASSERT (cur->status != THREAD_RUNNING);
    ASSERT (is_thread (next));

    if (cur != next)
        prev = switch_threads (cur, next);
    thread_schedule_tail (prev);
}


/* Return if a thread should wake up now */
    static bool
should_wakeup(struct list_elem *e, int64_t now) 
{
    if(e == list_end(&sleep_list)) return false;

    struct thread *t = list_entry(e, struct thread, sleep_elem);
    return t->wake_up_time <= now;
}


/* Returns a tid to use for a new thread. */
    static tid_t
allocate_tid (void) 
{
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire (&tid_lock);
    tid = next_tid++;
    lock_release (&tid_lock);

    return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
