/* Creates a "vine" of directories /0/1/2/3/4/5/6/7/8/9
   and changes directory into each of them,
   then creates a test file in the bottommost
   and verifies that it can be opened by absolute name. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  const char *file_name = "/0/1/2/3/4/5/6/7/8/9/test";
  char dir[2];

  dir[1] = '\0';
  for (dir[0] = '0'; dir[0] <= '9'; dir[0]++)
    {
      CHECK (mkdir (dir), "mkdir \"%s\"", dir);
      CHECK (chdir (dir), "chdir \"%s\"", dir);
    }
  CHECK (create ("test", 512), "create \"test\"");
  CHECK (chdir ("/"), "chdir \"/\"");
  CHECK (open (file_name) > 1, "open \"%s\"", file_name);
}

