#include <random.h>
#include <syscall.h>
#include "tests/filesys/extended/syn-rw.h"
#include "tests/lib.h"
#include "tests/main.h"

char buf[BUF_SIZE];

#define CHILD_CNT 4

void
test_main (void)
{
  pid_t children[CHILD_CNT];
  size_t ofs;
  int fd;

  CHECK (create (filename, 0), "create \"%s\"", filename);
  CHECK ((fd = open (filename)) > 1, "open \"%s\"", filename);

  exec_children ("child-syn-rw", children, CHILD_CNT);

  random_bytes (buf, sizeof buf);
  quiet = true;
  for (ofs = 0; ofs < BUF_SIZE; ofs += CHUNK_SIZE)
    CHECK (write (fd, buf + ofs, CHUNK_SIZE) > 0,
           "write %d bytes at offset %zu in \"%s\"",
           (int) CHUNK_SIZE, ofs, filename);
  quiet = false;

  wait_children (children, CHILD_CNT);
}
