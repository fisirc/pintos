#include <random.h>
#include <stdlib.h>
#include <syscall.h>
#include "tests/filesys/extended/syn-rw.h"
#include "tests/lib.h"

const char *test_name = "child-syn-rw";

static char buf1[BUF_SIZE];
static char buf2[BUF_SIZE];

int
main (int argc, const char *argv[])
{
  int child_idx;
  int fd;
  size_t ofs;

  quiet = true;

  CHECK (argc == 2, "argc must be 2, actually %d", argc);
  child_idx = atoi (argv[1]);

  random_init (0);
  random_bytes (buf1, sizeof buf1);

  CHECK ((fd = open (filename)) > 1, "open \"%s\"", filename);
  ofs = 0;
  while (ofs < sizeof buf2)
    {
      int bytes_read = read (fd, buf2 + ofs, sizeof buf2 - ofs);
      CHECK (bytes_read >= -1 && bytes_read <= (int) (sizeof buf2 - ofs),
             "%zu-byte read on \"%s\" returned invalid value of %d",
             sizeof buf2 - ofs, filename, bytes_read);
      if (bytes_read > 0)
        {
          compare_bytes (buf2 + ofs, buf1 + ofs, bytes_read, ofs, filename);
          ofs += bytes_read;
        }
    }
  close (fd);

  return child_idx;
}
