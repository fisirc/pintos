#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

char expected[] = {
  "Amazing Electronic Fact: If you scuffed your feet long enough without\n"
  "touching anything, you would build up so many electrons that your\n"
  "finger would explode!  But this is nothing to worry about unless you\n"
  "have carpeting.\n"
};



static char *
mk_boundary_string (const char *src)
{
  static char dst[8192];
  char *p = dst + (4096 - (uintptr_t) dst % 4096 - strlen (src) / 2);
  strlcpy (p, src, 4096);
  return p;
}

int
main (void)
{
  int handle;
  int byte_cnt;
  char *actual_p;

  actual_p = mk_boundary_string (expected);

  printf ("(read-boundary) begin\n");

  handle = open ("sample.txt");
  if (handle < 2)
    printf ("(read-boundary) fail: open() returned %d\n", handle);

  byte_cnt = read (handle, actual_p, sizeof expected - 1);
  if (byte_cnt != sizeof expected - 1)
    printf ("(read-boundary) fail: read() returned %d instead of %d\n",
            byte_cnt, sizeof expected - 1);
  else if (strcmp (expected, actual_p))
    printf ("(read-boundary) fail: expected text differs from actual:\n%s",
            actual_p);

  printf ("(read-boundary) end\n");
  return 0;
}
