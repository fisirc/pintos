#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* 🧠 project3/vm
  A Frame Table Entry (FTE)

  kpage: kernel virtual address
  upage: user virtual address
  t: thread that owns the frame
  list_elem: list element for frame table
*/
struct fte
  {
    void *kpage;
    void *upage;

    struct thread *t;

    struct list_elem list_elem;
  };

/* 🧠 project3/vm */

void frame_init (void);
void *falloc_get_page (enum palloc_flags, void *);
void falloc_free_page (void *);
struct fte *get_fte (void* );

#endif
