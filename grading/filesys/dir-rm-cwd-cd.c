#include <syscall.h>
#include "fslib.h"

const char test_name[] = "dir-rm-cwd-cd";

void
test_main (void)
{
  CHECK (mkdir ("a"), "mkdir \"a\"");
  CHECK (chdir ("a"), "chdir \"a\"");
  msg ("remove \"/a\" (must not crash)");
  if (remove ("/a"))
    CHECK (!chdir ("/a"),
           "chdir \"/a\" (remove succeeded so this must return false)");
  else
    CHECK (chdir ("/a"), "chdir \"/a\" (remove failed so this must succeed)");
}
