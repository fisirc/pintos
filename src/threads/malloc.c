#include "malloc.h"
#include <stdint.h>
#include "mmu.h"
#include "palloc.h"
#include "synch.h"
#include "lib/debug.h"
#include "lib/lib.h"
#include "lib/list.h"

/* A simple implementation of malloc().

   The size of each request, in bytes, is rounded up to a power
   of 2 and assigned to the "descriptor" that manages blocks of
   that size.  The descriptor keeps a list of free blocks.  If
   the free list is nonempty, one of its blocks is used to
   satisfy the request.

   Otherwise, a new page of memory, called an "arena", is
   obtained from the page allocator (if none is available,
   malloc() returns a null pointer).  The new arena is divided
   into blocks, all of which are added to the descriptor's free
   list.  Then we return one of the new blocks.

   When we free a block, we add it to its descriptor's free list.
   But if the arena that the block was in now has no in-use
   blocks, we remove all of the arena's blocks from the free list
   and give the arena back to the page allocator.

   Major limitation: the largest block that can be allocated is
   PGSIZE / 2, or 2 kB.  Use palloc_get() to allocate pages (4 kB
   blocks).  You're on your own if you need more memory than
   that. */

/* Descriptor. */
struct desc
  {
    size_t block_size;          /* Size of each element in bytes. */
    size_t blocks_per_arena;    /* Number of blocks in an arena. */
    struct list free_list;      /* List of free blocks. */
    struct lock lock;           /* Lock. */
  };

/* Arena. */
struct arena
  {
    struct desc *desc;          /* Owning descriptor. */
    size_t free_cnt;            /* Number of free blocks. */
  };

/* Free block. */
struct block
  {
    list_elem free_elem;        /* Free list element. */
  };

/* Our set of descriptors. */
static struct desc descs[16];   /* Descriptors. */
static size_t desc_cnt;         /* Number of descriptors. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *, size_t idx);

/* Initializes the malloc() descriptors. */
void
malloc_init (void)
{
  size_t block_size;

  for (block_size = 16; block_size < PGSIZE; block_size *= 2)
    {
      struct desc *d = &descs[desc_cnt++];
      ASSERT (desc_cnt <= sizeof descs / sizeof *descs);
      d->block_size = block_size;
      d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
      list_init (&d->free_list);
      lock_init (&d->lock, "malloc");
    }
}

/* Obtains and returns a new block of at least SIZE bytes.
   Returns a null pointer if memory is not available. */
void *
malloc (size_t size)
{
  struct desc *d;
  struct block *b;
  struct arena *a;

  /* A null pointer satisfies a request for 0 bytes. */
  if (size == 0)
    return NULL;

  /* Find the smallest descriptor that satisfies a SIZE-byte
     request. */
  for (d = descs; d < descs + desc_cnt; d++)
    if (size < d->block_size)
      break;
  if (d == descs + desc_cnt)
    {
      printk ("malloc: %zu byte allocation too big\n", size);
      return NULL;
    }

  lock_acquire (&d->lock);

  /* If the free list is empty, create a new arena. */
  if (list_empty (&d->free_list))
    {
      size_t i;

      /* Allocate a page. */
      a = palloc_get (0);
      if (a == NULL)
        {
          lock_release (&d->lock);
          return NULL;
        }

      /* Initialize arena and add its blocks to the free list. */
      a->desc = d;
      a->free_cnt = d->blocks_per_arena;
      for (i = 0; i < d->blocks_per_arena; i++)
        {
          b = arena_to_block (a, i);
          list_push_back (&d->free_list, &b->free_elem);
        }
    }

  /* Get a block from free list and return it. */
  b = list_entry (list_pop_front (&d->free_list), struct block, free_elem);
  a = block_to_arena (b);
  a->free_cnt--;
  lock_release (&d->lock);
  return b;
}

/* Allocates and return A times B bytes initialized to zeroes.
   Returns a null pointer if memory is not available. */
void *
calloc (size_t a, size_t b)
{
  void *p;
  size_t size;

  /* Calculate block size. */
  size = a * b;
  if (size < a || size < b)
    return NULL;

  /* Allocate and zero memory. */
  p = malloc (size);
  if (p != NULL)
    memset (p, 0, size);

  return p;
}

/* Frees block P, which must have been previously allocated with
   malloc() or calloc(). */
void
free (void *p)
{
  struct block *b = p;
  struct arena *a = block_to_arena (b);
  struct desc *d = a->desc;

  if (p == NULL)
    return;

  lock_acquire (&d->lock);

  /* Add block to free list. */
  list_push_front (&d->free_list, &b->free_elem);

  /* If the arena is now entirely unused, free it. */
  if (++a->free_cnt >= d->blocks_per_arena)
    {
      size_t i;

      ASSERT (a->free_cnt == d->blocks_per_arena);
      for (i = 0; i < d->blocks_per_arena; i++)
        {
          struct block *b = arena_to_block (a, i);
          list_remove (&b->free_elem);
        }
      palloc_free (a);
    }

  lock_release (&d->lock);
}

/* Returns the arena that block B is inside. */
static struct arena *
block_to_arena (struct block *b)
{
  return pg_round_down (b);
}

/* Returns the (IDX - 1)'th block within arena A. */
static struct block *
arena_to_block (struct arena *a, size_t idx)
{
  ASSERT (idx < a->desc->blocks_per_arena);
  return (struct block *) ((uint8_t *) a
                           + sizeof *a
                           + idx * a->desc->block_size);
}
