/* Problem 1-1: Alarm Clock tests.

   Tests timer_sleep(0).  Only requirement is that it not crash. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_alarm_zero (void)
{
  timer_sleep (0);
  pass ();
}
