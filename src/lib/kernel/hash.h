#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* Hash table.

   This is a standard hash table with chaining.  To locate an
   element in the table, we compute a hash function over the
   element's data and use that as an index into an array of
   doubly linked lists, then linearly search the list.

   The chain lists do not use dynamic allocation.  Instead, each
   structure that can potentially be in a hash must embed a
   hash_elem member.  All of the hash functions operate on these
   `hash_elem's.  The hash_entry macro allows conversion from a
   hash_elem back to a structure object that contains it.  This
   is the same technique used in the linked list implementation.
   Refer to lib/kernel/list.h for a detailed explanation.

   The FAQ for the VM project contains a detailed example of how
   to use the hash table. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
typedef list_elem hash_elem;

/* Converts pointer to hash element HASH_ELEM into a pointer to
   the structure that HASH_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the hash element.  See the big comment at the top of the
   file for an example. */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                              \
        ((STRUCT *) ((uint8_t *) (HASH_ELEM) - offsetof (STRUCT, MEMBER)))

/* Computes and returns the hash value for hash element E, given
   auxiliary data AUX. */
typedef unsigned hash_hash_func (const hash_elem *e, void *aux);

/* Compares the value of two hash elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool hash_less_func (const hash_elem *a, const hash_elem *b,
                             void *aux);

/* Hash table. */
struct hash
  {
    size_t elem_cnt;            /* Number of elements in table. */
    size_t bucket_cnt;          /* Number of buckets, a power of 2. */
    struct list *buckets;       /* Array of `bucket_cnt' lists. */
    hash_hash_func *hash;       /* Hash function. */
    hash_less_func *less;       /* Comparison function. */
    void *aux;                  /* Auxiliary data for `hash' and `less'. */
  };

/* A hash table iterator. */
struct hash_iterator
  {
    struct hash *hash;          /* The hash table. */
    struct list *bucket;        /* Current bucket. */
    hash_elem *elem;            /* Current hash element in current bucket. */
  };

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *);
void hash_destroy (struct hash *);

/* Search, insertion, deletion. */
hash_elem *hash_insert (struct hash *, hash_elem *);
hash_elem *hash_replace (struct hash *, hash_elem *);
hash_elem *hash_find (struct hash *, hash_elem *);
hash_elem *hash_delete (struct hash *, hash_elem *);

/* Iteration. */
void hash_first (struct hash_iterator *, struct hash *);
hash_elem *hash_next (struct hash_iterator *);
hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
unsigned hash_bytes (const void *, size_t);
unsigned hash_string (const char *);
unsigned hash_int (int);

#endif /* lib/kernel/hash.h */
