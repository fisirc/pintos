#include "threads/init.h"
#include <console.h>
#include <debug.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devices/kbd.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/paging.h"
#include "threads/palloc.h"
#include "threads/test.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#endif
#ifdef FILESYS
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif

/* Amount of physical memory, in 4 kB pages. */
size_t ram_pages;

#ifdef FILESYS
/* Format the filesystem? */
static bool format_filesys;
#endif

#ifdef USERPROG
/* Initial program to run. */
static char *initial_program;
#endif

/* Power off after running requested actions? */
static bool power_off;

static void ram_init (void);
static void argv_init (void);
static void do_power_off (void);

int main (void) NO_RETURN;

int
main (void)
{
  /* Initialize everything needed for printf() first. */
  ram_init ();
  thread_init ();
  vga_init ();
  serial_init_poll ();
  console_init ();

  /* Greet user. */
  printf ("Pintos booting with %'zd kB RAM...\n", ram_pages * (PGSIZE / 1024));

  /* Parse command line. */
  argv_init ();

  /* Initialize memory system, segments, paging. */
  palloc_init ();
  paging_init ();
#ifdef USERPROG
  tss_init ();
  gdt_init ();
#endif
  malloc_init ();

  /* Set random seed if argv_init() didn't. */
  random_init (0);

  /* Initialize interrupt handlers. */
  intr_init ();
  timer_init ();
  kbd_init ();
#ifdef USERPROG
  exception_init ();
  syscall_init ();
#endif

  /* Start thread scheduler and enable interrupts. */
  thread_start ();
  serial_init_queue ();

#ifdef FILESYS
  /* Initialize filesystem. */
  disk_init ();
  filesys_init (format_filesys);
  fsutil_run ();
#endif

  printf ("Boot complete.\n");

#ifdef USERPROG
  /* Run a user program. */
  if (initial_program != NULL)
    {
      printf ("\nExecuting '%s':\n", initial_program);
      thread_execute (initial_program);
    }
#else
  test ();
#endif

  if (power_off)
    do_power_off ();

  /* Terminate this thread. */
  thread_exit ();
}

/* Clear BSS and obtain RAM size from loader. */
static void
ram_init (void)
{
  /* The "BSS" is a segment that should be initialized to zeros.
     It isn't actually stored on disk or zeroed by the kernel
     loader, so we have to zero it ourselves.

     The start and end of the BSS segment is recorded by the
     linker as _start_bss and _end_bss.  See kernel.lds. */
  extern char _start_bss, _end_bss;
  memset (&_start_bss, 0, &_end_bss - &_start_bss);

  /* Get RAM size from loader.  See loader.S. */
  ram_pages = *(uint32_t *) ptov (LOADER_RAM_PAGES);
}

/* Parses the command line. */
static void
argv_init (void)
{
  char *cmd_line, *pos;
  char *argv[LOADER_CMD_LINE_LEN / 2 + 2];
  int argc = 0;
  int i;

  /* The command line is made up of null terminated strings
     followed by an empty string.  Break it up into words. */
  cmd_line = pos = ptov (LOADER_CMD_LINE);
  printf ("Kernel command line:");
  while (pos < cmd_line + LOADER_CMD_LINE_LEN)
    {
      ASSERT (argc < LOADER_CMD_LINE_LEN / 2);
      if (*pos == '\0')
        break;
      argv[argc++] = pos;
      printf (" %s", pos);
      pos = strchr (pos, '\0') + 1;
    }
  printf ("\n");
  argv[argc] = "";
  argv[argc + 1] = "";

  /* Parse the words. */
  for (i = 0; i < argc; i++)
    if (!strcmp (argv[i], "-rs"))
      random_init (atoi (argv[++i]));
    else if (!strcmp (argv[i], "-d"))
      debug_enable (argv[++i]);
    else if (!strcmp (argv[i], "-q"))
      power_off = true;
#ifdef USERPROG
    else if (!strcmp (argv[i], "-ex"))
      initial_program = argv[++i];
#endif
#ifdef FILESYS
  else if (!strcmp (argv[i], "-f"))
      format_filesys = true;
    else if (!strcmp (argv[i], "-ci"))
      {
        fsutil_copyin_file = argv[++i];
        fsutil_copyin_size = atoi (argv[++i]);
      }
    else if (!strcmp (argv[i], "-co"))
      fsutil_copyout_file = argv[++i];
    else if (!strcmp (argv[i], "-p"))
      fsutil_print_file = argv[++i];
    else if (!strcmp (argv[i], "-r"))
      fsutil_remove_file = argv[++i];
    else if (!strcmp (argv[i], "-ls"))
      fsutil_list_files = true;
    else if (!strcmp (argv[i], "-D"))
      fsutil_dump_filesys = true;
#endif
    else if (!strcmp (argv[i], "-u"))
      {
        printf (
          "Kernel options:\n"
          " -rs SEED            Seed random seed to SEED.\n"
          " -d CLASS[,...]      Enable the given classes of debug messages.\n"
#ifdef USERPROG
          " -ex 'PROG [ARG...]' Run PROG, passing the optional arguments.\n"
#endif
#ifdef FILESYS
          " -f                  Format the filesystem disk (hdb or hd0:1).\n"
          " -ci FILENAME SIZE   Copy SIZE bytes from the scratch disk (hdc\n"
          "                     or hd1:0) into the filesystem as FILENAME\n"
          " -co FILENAME        Copy FILENAME to the scratch disk, with\n"
          "                     size at start of sector 0 and data afterward\n"
          " -p FILENAME         Print the contents of FILENAME\n"
          " -r FILENAME         Delete FILENAME\n"
          " -ls                 List the files in the filesystem\n"
          " -D                  Dump complete filesystem contents\n"
#endif
          " -q                  Power off after doing requested actions.\n"
          " -u                  Print this help message and power off.\n"
          );
        do_power_off ();
      }
    else
      PANIC ("unknown option `%s' (use -u for help)", argv[i]);
}

void
do_power_off (void)
{
  const char s[] = "Shutdown";
  const char *p;

#ifdef FILESYS
  filesys_done ();
#endif

  printf ("Powering off...\n");
  for (p = s; *p != '\0'; p++)
    outb (0x8900, *p);
  for (;;);
}
