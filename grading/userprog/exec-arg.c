#include <stdio.h>
#include <syscall.h>

int
main (void)
{
  printf ("(exec-arg) begin\n");
  join (exec ("child-arg childarg"));
  printf ("(exec-arg) end\n");
  return 0;
}
