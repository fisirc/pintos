#include "thread.h"
#include <stddef.h>
#include "debug.h"
#include "interrupt.h"
#include "intr-stubs.h"
#include "lib.h"
#include "mmu.h"
#include "palloc.h"
#include "random.h"
#include "switch.h"
#ifdef USERPROG
#include "gdt.h"
#endif

/* Value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0x1234abcdu

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list run_queue;

/* Idle thread. */
static struct thread *idle_thread;      /* Thread. */
static void idle (void *aux UNUSED);    /* Thread function. */

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

static void kernel_thread (thread_func *, void *aux);

static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static struct thread *new_thread (const char *name);
static void init_thread (struct thread *, const char *name);
static bool is_thread (struct thread *);
static void *alloc_frame (struct thread *, size_t size);
static void destroy_thread (struct thread *);
static void schedule (void);
void schedule_tail (struct thread *prev);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  Note that this is
   possible only because the loader was careful to put the bottom
   of the stack at a page boundary; it won't work in general.
   Also initializes the run queue.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create(). */
void
thread_init (void)
{
  struct thread *t;

  ASSERT (intr_get_level () == INTR_OFF);

  /* Set up a thread structure for the running thread. */
  t = running_thread ();
  init_thread (t, "main");
  t->status = THREAD_RUNNING;

  /* Initialize run queue. */
  list_init (&run_queue);
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create idle thread. */
  idle_thread = thread_create ("idle", idle, NULL);
  idle_thread->status = THREAD_BLOCKED;

  /* Enable interrupts. */
  intr_enable ();
}

/* Creates a new kernel thread named NAME, which executes
   FUNCTION passing AUX as the argument, and adds it to the ready
   queue.  If thread_start() has been called, then the new thread
   may be scheduled before thread_create() returns.  Use a
   semaphore or some other form of synchronization if you need to
   ensure ordering. */
struct thread *
thread_create (const char *name, thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;

  ASSERT (function != NULL);

  t = new_thread (name);

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

  /* Add to run queue. */
  thread_unblock (t);

  return t;
}

#ifdef USERPROG
/* Starts a new thread running a user program loaded from
   FILENAME, and adds it to the ready queue.  If thread_start()
   has been called, then new thread may be scheduled before
   thread_execute() returns.*/
bool
thread_execute (const char *filename)
{
  struct thread *t;
  struct intr_frame *if_;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  void (*start) (void);

  ASSERT (filename != NULL);

  t = new_thread (filename);
  if (t == NULL)
    return false;

  if (!addrspace_load (t, filename, &start))
    PANIC ("%s: program load failed", filename);

  /* Interrupt frame. */
  if_ = alloc_frame (t, sizeof *if_);
  if_->es = SEL_UDSEG;
  if_->ds = SEL_UDSEG;
  if_->eip = start;
  if_->cs = SEL_UCSEG;
  if_->eflags = FLAG_IF | FLAG_MBS;
  if_->esp = PHYS_BASE;
  if_->ss = SEL_UDSEG;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = intr_exit;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;

  /* Add to run queue. */
  thread_unblock (t);

  return true;
}
#endif

/* Transitions a blocked thread T from its current state to the
   ready-to-run state.  If T is not blocked, there is no effect.
   (Use thread_yield() to make the running thread ready.) */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  if (t->status == THREAD_BLOCKED)
    {
      list_push_back (&run_queue, &t->elem);
      t->status = THREAD_READY;
    }
  intr_set_level (old_level);
}

/* Returns the name of thread T. */
const char *
thread_name (struct thread *t)
{
  ASSERT (is_thread (t));
  return t->name;
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

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
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
  list_push_back (&run_queue, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
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

/* Idle thread.  Executes when no other thread is ready to run. */
static void
idle (void *aux UNUSED)
{
  for (;;)
    {
      /* Wait for an interrupt. */
      DEBUG (idle, "idle");
      asm ("hlt");

      /* Let someone else run. */
      intr_disable ();
      thread_block ();
      intr_enable ();
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
  asm ("movl %%esp, %0\n" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Creates a new thread named NAME and initializes its fields.
   Returns the new thread if successful or a null pointer on
   failure. */
static struct thread *
new_thread (const char *name)
{
  struct thread *t;

  ASSERT (name != NULL);

  t = palloc_get (PAL_ZERO);
  if (t != NULL)
    init_thread (t, name);

  return t;
}

/* Initializes T as a new, blocked thread named NAME. */
static void
init_thread (struct thread *t, const char *name)
{
  memset (t, 0, sizeof *t);
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->status = THREAD_BLOCKED;
  t->magic = THREAD_MAGIC;
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
  if (list_empty (&run_queue))
    return idle_thread;
  else
    return list_entry (list_pop_front (&run_queue), struct thread, elem);
}

/* Destroys T, which must be in the dying state and must not be
   the running thread. */
static void
destroy_thread (struct thread *t)
{
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_DYING);
  ASSERT (t != thread_current ());

#ifdef USERPROG
  addrspace_destroy (t);
#endif
  palloc_free (t);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

#ifdef USERPROG
  /* Activate the new address space. */
  addrspace_activate (cur);
#endif

  /* If the thread we switched from is dying, destroy it.
     This must happen late because it's not a good idea to
     e.g. destroy the page table you're currently using. */
  if (prev != NULL && prev->status == THREAD_DYING)
    destroy_thread (prev);
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it. */
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
  schedule_tail (prev);
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
