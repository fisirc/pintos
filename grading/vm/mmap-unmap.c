#include <stdio.h>
#include <string.h>
#ifdef PINTOS
#include <syscall.h>
#else
#include "posix-compat.h"
#endif
#include "sample.inc"

#define ACTUAL ((void *) 0x10000000)

int
main (void)
{
  int fd;
  mapid_t map;

  printf ("(mmap-unmap) begin\n");

  fd = open ("sample.txt");
  if (fd < 0)
    {
      printf ("(mmap-unmap) open() failed\n");
      return 1;
    }

  map = mmap (fd, ACTUAL);
  if (map == MAP_FAILED)
    {
      printf ("(mmap-unmap) mmap() failed\n");
      return 1;
    }

  munmap (map);

  printf ("(mmap-unmap) FAIL: unmapped memory is readable (%d)\n",
          *(int *) ACTUAL);

  /* Done. */
  printf ("(mmap-unmap) end\n");

  return 0;
}
