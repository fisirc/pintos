#include "vm/frame.h"
#include "threads/synch.h"
#include "vm/swap.h"

// 🧠 project3/vm
// Frame frame table and synchronization
static struct list frame_table;
static struct lock frame_lock;
static struct fte *clock_cursor; // clock algorithm: pointer to the current frame

// 🧠 project3/vm
// Frame table initialization
void
frame_init ()
{
  list_init (&frame_table);
  lock_init (&frame_lock);
  clock_cursor = NULL;
}

// 🧠 project3/vm
// Receives the flags that will be used for page_alloc
//
// falloc is responsible for allocating frames for pages
// It uses the palloc_get_page function to get a frame
// If there is no frame available, it calls the evict_page function
// to free a frame and then tries to allocate it again
void *
falloc_get_page(enum palloc_flags flags, void *upage)
{
  struct fte *e;
  void *kpage;
  lock_acquire (&frame_lock);
  kpage = palloc_get_page (flags);

  if (kpage == NULL)
    {
      evict_page();
      kpage = palloc_get_page (flags);
      if (kpage == NULL)
        return NULL;
    }

  e = (struct fte *)malloc (sizeof *e);
  e->kpage = kpage;
  e->upage = upage;
  e->t = thread_current ();
  list_push_back (&frame_table, &e->list_elem);

  lock_release (&frame_lock);
  return kpage;
}

// 🧠 project3/vm
// Free the frame and remove it from the frame table
void
falloc_free_page (void *kpage)
{
  struct fte *e;
  lock_acquire (&frame_lock);
  e = get_fte (kpage);

  if (e == NULL)
    sys_exit (-1); // In future versions we may want to treat this error
                   // in a more elegant way

  list_remove (&e->list_elem);
  palloc_free_page (e->kpage);
  // FIX: also remove the page from the page table
  pagedir_clear_page (e->t->pagedir, e->upage);
  free (e);
  lock_release (&frame_lock);
}

// 🧠 project3/vm
// Get the frame table entry for a given frame
struct fte *
get_fte (void* kpage)
{
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
    if (list_entry (e, struct fte, list_elem)->kpage == kpage)
      return list_entry (e, struct fte, list_elem);

  return NULL;
}

// 🧠 project3/vm
// Eviction policy based on the clock algorithm
void
evict_page () {
  ASSERT(lock_held_by_current_thread(&frame_lock));

  struct fte *e = clock_cursor;
  struct spte *s;

  // We find a page to evict
  do
    {
      if (e != NULL)
        pagedir_set_accessed(e->t->pagedir, e->upage, false);

      if (clock_cursor == NULL || list_next (&clock_cursor->list_elem) == list_end (&frame_table))
        e = list_entry(list_begin(&frame_table), struct fte, list_elem);
      else
        e = list_next (e);

    } while (!pagedir_is_accessed(e->t->pagedir, e->upage));

  // And now we evict it

  s = get_spte (&thread_current()->spt, e->upage);
  s->status = PAGE_SWAP;
  s->swap_id = swap_out (e->kpage); // swap_out returns the swap id

  lock_release (&frame_lock);
  falloc_free_page (e->kpage);
  lock_acquire (&frame_lock);
}
