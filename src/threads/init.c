#include "init.h"
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "debug.h"
#include "gdt.h"
#include "interrupt.h"
#include "io.h"
#include "kbd.h"
#include "lib.h"
#include "loader.h"
#include "malloc.h"
#include "mmu.h"
#include "paging.h"
#include "palloc.h"
#include "random.h"
#include "serial.h"
#include "thread.h"
#include "timer.h"
#include "vga.h"
#ifdef FILESYS
#include "filesys.h"
#include "disk.h"
#include "fsutil.h"
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

static void ram_init (void);
static void argv_init (void);

static void
main_thread (void *aux UNUSED)
{
#ifdef FILESYS
  disk_init ();
  filesys_init (format_filesys);
  fsutil_run ();
#endif

#ifdef USERPROG
  if (initial_program != NULL)
    thread_execute (initial_program);
  else
    PANIC ("no initial program specified");
#endif
}

int
main (void)
{
  /* Initialize prerequisites for calling printk(). */
  ram_init ();
  vga_init ();
  serial_init ();

  /* Greet user. */
  printk ("Booting cnachos86 with %'d kB RAM...\n", ram_pages * 4);

  /* Parse command line. */
  argv_init ();

  /* Initialize memory system. */
  palloc_init ();
  paging_init ();
  gdt_init ();
  malloc_init ();

  random_init (0);

  /* Initialize interrupt handlers. */
  intr_init ();
  timer_init ();
  kbd_init ();

  /* Do everything else in a system thread. */
  thread_init ();
  thread_create ("main", main_thread, NULL);
  thread_start ();
}


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

  /* Get RAM size from loader. */
  ram_pages = *(uint32_t *) ptov (LOADER_RAM_PAGES);
}

static void
argv_init (void)
{
  char *cmd_line, *pos;
  char *argv[LOADER_CMD_LINE_LEN / 2 + 1];
  int argc = 0;
  int i;

  /* The command line is made up of null terminated strings
     followed by an empty string.  Break it up into words. */
  cmd_line = pos = ptov (LOADER_CMD_LINE);
  while (pos < cmd_line + LOADER_CMD_LINE_LEN)
    {
      ASSERT (argc < LOADER_CMD_LINE_LEN / 2);
      if (*pos == '\0')
        break;
      argv[argc++] = pos;
      pos = strchr (pos, '\0') + 1;
    }
  argv[argc] = "";

  /* Parse the words. */
  for (i = 0; i < argc; i++)
    if (!strcmp (argv[i], "-rs"))
      random_init (atoi (argv[++i]));
    else if (!strcmp (argv[i], "-d"))
      debug_enable (argv[++i]);
#ifdef USERPROG
    else if (!strcmp (argv[i], "-ex"))
      initial_program = argv[++i];
#endif
#ifdef FILESYS
  else if (!strcmp (argv[i], "-f"))
      format_filesys = true;
    else if (!strcmp (argv[i], "-cp"))
      fsutil_copy_arg = argv[++i];
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
        printk (
          "Kernel options:\n"
          " -rs SEED            Seed random seed to SEED.\n"
          " -d CLASS[,...]      Enable the given classes of debug messages.\n"
#ifdef USERPROG
          " -ex 'PROG [ARG...]' Run PROG, passing the optional arguments.\n"
#endif
#ifdef FILESYS
          " -f                  Format the filesystem disk (hdb or hd0:1).\n"
          " -cp FILENAME:SIZE   Copy SIZE bytes from the scratch disk (hdc\n"
          "                     or hd1:0) into the filesystem as FILENAME\n"
          " -p FILENAME         Print the contents of FILENAME\n"
          " -r FILENAME         Delete FILENAME\n"
          " -ls                 List the files in the filesystem\n"
          " -D                  Dump complete filesystem contents\n");
#endif
      }
    else
      PANIC ("unknown option `%s'", argv[i]);
}
