#include "palloc.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "mmu.h"

/* A free page owned by the page allocator. */
struct page
  {
    struct page *next;  /* Next free page, or null at end of chain. */
  };

static struct page *free_pages;
static uint8_t *uninit_start, *uninit_end;

void
palloc_init (uint8_t *start, uint8_t *end)
{
  uninit_start = start;
  uninit_end = end;
}

void *
palloc_get (enum palloc_flags flags)
{
  struct page *page;

  if (free_pages == NULL && uninit_start < uninit_end)
    {
      palloc_free (uninit_start);
      uninit_start += NBPG;
    }

  page = free_pages;
  if (page != NULL)
    {
      free_pages = page->next;
      if (flags & PAL_ZERO)
        memset (page, 0, NBPG);
    }
  else
    {
      if (flags & PAL_ASSERT)
        panic ("palloc_get: out of pages");
    }

  return page;
}

void
palloc_free (void *page_)
{
  struct page *page = page_;
  ASSERT((uintptr_t) page % NBPG == 0);
#ifndef NDEBUG
  memset (page, 0xcc, NBPG);
#endif
  page->next = free_pages;
  free_pages = page;
}
