#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#ifndef USERPROG
#define USERPROG
#endif

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"

#include "synch.h"
#include <hash.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* 👤 project2/userprog
   Process Control Block (PCB)
   Created to store general information about the process that
   is not thread-specific. */
struct pcb
  {
    int exit_code;
    bool has_exited;
    bool has_loaded;

    struct file **fd_table; // File descriptor table
    int fd_count;           // Number of file descriptors
    struct file *exec_file;   // A reference to the opened executable
                            // of the process

    struct semaphore sema_wait; // Waiter for the process to finish
    struct semaphore sema_load; // Waiter for the process to be loaded
  };

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */

    int base_priority;                  /* 🧵 project1/task2
                                           Priority to be restored after
                                           priority donation */

    int64_t wakeup_tick;                /* 🧵 project1/task1
                                           The tick to wake up the thread */

    struct list donors;                 /* 🧵 project1/task2
                                           List of threads that have donated
                                           their priority to this thread */

    struct lock* waiting_for;           /* 🧵 project1/task2
                                           The lock the thread is waiting for */
    /* 🧵 project1/task3 */
    int nice;                           /* Thread's niceness value */
    int recent_cpu;                     /* Measure how much CPU time each
                                           thread has received “recently” */

    /* Shared between thread.c and synch.c.
       in ready_list, sleep_list, and as a semaphore waiters element. */
    struct list_elem elem;              /* List element. */

    struct list_elem allelem;           /* List element for all threads list. */
    struct list_elem donorelem;         /* 🧵 project1/task2
                                           List element for donors list. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct pcb *pcb;                    /* 👤 project2/userprog
                                           Process Control Block, stores
                                           information about the process */
    struct thread *parent_process;
    struct list list_child_process;
    struct list_elem elem_child_process;
#endif
    struct hash spt;                   /* 🧠 project3/vm
                                           Supplemental page table */
    void *esp;

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

// 🧵 project1/task1 definitions

void thread_sleep (int64_t ticks);
void thread_awake(int64_t ticks);
bool thread_wakeup_tick_less (const struct list_elem*, const struct list_elem*, void*);

// 🧵 project1/task2 definitions

bool thread_priority_desc (const struct list_elem*, const struct list_elem*, void*);
bool thread_priority_asc (const struct list_elem*, const struct list_elem*, void*);
void thread_sust (void);
void thread_recalculate_priority (struct thread *t);

// 🧵 project1/task3 definitions

void inc_recent_cpu (void);
void mlfqs_priority (struct thread *t);
void mlfqs_update_priority (void);
void mlfqs_update_recent_cpu (void);
void mlfqs_update_load_avg (void);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* 👤 project2/userprog */

struct thread *thread_get_child (tid_t child_tid);

#endif /* threads/thread.h */
