#ifndef THREADS_INIT_H
#define THREADS_INIT_H

#include <stddef.h>
#include <stdint.h>

/* Physical memory size, in 4 kB pages. */
extern size_t ram_pages;

/* Page directory with kernel mappings only. */
extern uint32_t *base_page_dir;

#endif /* threads/init.h */
