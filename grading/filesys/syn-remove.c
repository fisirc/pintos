#include <random.h>
#include <string.h>
#include <syscall.h>
#include "fslib.h"

const char test_name[] = "syn-remove";

char buf1[1234];
char buf2[1234];

void
test_main (void)
{
  const char *filename = "deleteme";
  int fd;

  check (create (filename, 0), "create \"%s\"", filename);
  check ((fd = open (filename)) > 1, "open \"%s\"", filename);
  check (remove (filename), "remove \"%s\"", filename);
  random_bytes (buf1, sizeof buf1);
  check (write (fd, buf1, sizeof buf1) > 0, "write \"%s\"", filename);
  msg ("seek \"%s\" to 0", filename);
  seek (fd, 0);
  check (read (fd, buf2, sizeof buf2) > 0, "read \"%s\"", filename);
  compare_bytes (buf2, buf1, sizeof buf1, 0, filename);
  msg ("close \"%s\"", filename);
  close (fd);
}
