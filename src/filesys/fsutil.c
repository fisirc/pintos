#include "fsutil.h"
#include <stdbool.h>
#include "file.h"
#include "filesys.h"
#include "lib/debug.h"
#include "lib/lib.h"
#include "threads/mmu.h"
#include "threads/palloc.h"

/* Filename and file size to use for copy operations,
   as "filename:size". */
char *fsutil_copy_arg;

/* Name of a file print to print to console. */
char *fsutil_print_file;

/* Name of a file to delete. */
char *fsutil_remove_file;

/* List all files in the filesystem to the system console? */
bool fsutil_list_files;

/* Dump full contents of filesystem to the system console? */
bool fsutil_dump_filesys;

/* Copies from the "scratch" disk, hdc or hd1:0,
   to a file named FILENAME in the filesystem.
   The file will be SIZE bytes in length. */
static void
copy (const char *filename, off_t size)
{
  struct disk *src;
  struct file dst;
  disk_sector_t sector;
  void *buffer;

  /* Open source disk. */
  src = disk_get (1, 0);
  if (src == NULL)
    PANIC ("couldn't open source disk (hdc or hd1:0)");
  if (size > (off_t) disk_size (src) * DISK_SECTOR_SIZE)
    PANIC ("source disk is too small for %lld-byte file",
           (unsigned long long) size);

  /* Create destination file. */
  if (!filesys_create (filename, size))
    PANIC ("%s: create failed", filename);
  if (!filesys_open (filename, &dst))
    PANIC ("%s: open failed", filename);

  /* Do copy. */
  buffer = palloc_get (PAL_ASSERT);
  sector = 0;
  while (size > 0)
    {
      int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
      disk_read (src, sector++, buffer);
      if (file_write (&dst, buffer, chunk_size) != chunk_size)
        PANIC ("%s: write failed with %lld bytes unwritten",
               filename, (unsigned long long) size);
      size -= chunk_size;
    }
  palloc_free (buffer);

  file_close (&dst);
}

/* Executes the filesystem operations described by the variables
   declared in fsutil.h. */
void
fsutil_run (void)
{
  if (fsutil_copy_arg != NULL)
    {
      char *save;
      char *filename = strtok_r (fsutil_copy_arg, ":", &save);
      char *size = strtok_r (NULL, "", &save);

      if (filename == NULL || size == NULL)
        PANIC ("bad format for -cp option; use -u for usage");

      copy (filename, atoi (size));
    }

  if (fsutil_print_file != NULL)
    fsutil_print (fsutil_print_file);

  if (fsutil_remove_file != NULL)
    {
      if (filesys_remove (fsutil_remove_file))
        printk ("%s: removed\n", fsutil_remove_file);
      else
        PANIC ("%s: remove failed\n", fsutil_remove_file);
    }

  if (fsutil_list_files)
    filesys_list ();

  if (fsutil_dump_filesys)
    filesys_dump ();
}

/* Prints the contents of file FILENAME to the system console as
   hex and ASCII. */
void
fsutil_print (const char *filename)
{
  struct file file;
  char *buffer;

  if (!filesys_open (filename, &file))
    PANIC ("%s: open failed", filename);
  buffer = palloc_get (PAL_ASSERT);
  for (;;)
    {
      off_t n = file_read (&file, buffer, PGSIZE);
      if (n == 0)
        break;

      hex_dump (buffer, n, true);
    }
  palloc_free (buffer);
  file_close (&file);
}
