#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/page.h"

/* 🧠 project3/vm */

void init_swap_valid_table (void);
void swap_in(struct spte *page, void *kva);
int swap_out(void *kva);

#endif