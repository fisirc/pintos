#include "fslib.h"
#include <random.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

bool quiet = false;

static void
vmsg (const char *format, va_list args, const char *suffix)
{
  /* We go to some trouble to stuff the entire message into a
     single buffer and output it in a single system call, because
     that'll (typically) ensure that it gets sent to the console
     atomically.  Otherwise kernel messages like "foo: exit(0)"
     can end up being interleaved if we're unlucky. */
  static char buf[1024];

  snprintf (buf, sizeof buf, "(%s) ", test_name);
  vsnprintf (buf + strlen (buf), sizeof buf - strlen (buf), format, args);
  strlcpy (buf + strlen (buf), suffix, sizeof buf - strlen (buf));
  write (STDOUT_FILENO, buf, strlen (buf));
}

void
msg (const char *format, ...)
{
  va_list args;

  if (quiet)
    return;
  va_start (args, format);
  vmsg (format, args, "\n");
  va_end (args);
}

void
fail (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  vmsg (format, args, ": FAILED\n");
  va_end (args);

  exit (1);
}

void
seq_test (const char *filename, void *buf, size_t size, size_t initial_size,
          size_t (*block_size_func) (void),
          void (*check_func) (int fd, long ofs))
{
  size_t ofs;
  int fd;

  random_bytes (buf, size);
  check (create (filename, initial_size), "create \"%s\"", filename);
  check ((fd = open (filename)) > 1, "open \"%s\"", filename);

  ofs = 0;
  msg ("writing \"%s\"", filename);
  while (ofs < size)
    {
      size_t block_size = block_size_func ();
      if (block_size > size - ofs)
        block_size = size - ofs;

      if (write (fd, buf + ofs, block_size) <= 0)
        fail ("write %zu bytes at offset %zu in \"%s\" failed",
              block_size, ofs, filename);

      ofs += block_size;
      if (check_func != NULL)
        check_func (fd, ofs);
    }
  msg ("close \"%s\"", filename);
  close (fd);
  check_file (filename, buf, size);
}

static void
swap (void *a_, void *b_, size_t size)
{
  uint8_t *a = a_;
  uint8_t *b = b_;
  size_t i;

  for (i = 0; i < size; i++)
    {
      uint8_t t = a[i];
      a[i] = b[i];
      b[i] = t;
    }
}

void
shuffle (void *buf_, size_t cnt, size_t size)
{
  char *buf = buf_;
  size_t i;

  for (i = 0; i < cnt; i++)
    {
      size_t j = i + random_ulong () % (cnt - i);
      swap (buf + i * size, buf + j * size, size);
    }
}

void
check_file (const char *filename, const void *buf_, size_t size)
{
  const char *buf = buf_;
  size_t ofs;
  char block[512];
  int fd;

  check ((fd = open (filename)) > 1, "open \"%s\" for verification", filename);

  ofs = 0;
  while (ofs < size)
    {
      size_t block_size = size - ofs;
      if (block_size > sizeof block)
        block_size = sizeof block;

      if (read (fd, block, block_size) <= 0)
        fail ("read %zu bytes at offset %zu in \"%s\" failed",
              block_size, ofs, filename);

      compare_bytes (block, buf + ofs, block_size, ofs, filename);
      ofs += block_size;
    }

  msg ("close \"%s\"", filename);
  close (fd);
}

void
compare_bytes (const void *read_data_, const void *expected_data_, size_t size,
               size_t ofs, const char *filename)
{
  const uint8_t *read_data = read_data_;
  const uint8_t *expected_data = expected_data_;
  size_t i, j;
  size_t show_cnt;

  if (!memcmp (read_data, expected_data, size))
    return;

  for (i = 0; i < size; i++)
    if (read_data[i] != expected_data[i])
      break;
  for (j = i + 1; j < size; j++)
    if (read_data[j] == expected_data[j])
      break;

  quiet = false;
  msg ("%zu bytes read starting at offset %zu in \"%s\" differ "
       "from expected.", j - i, ofs + i, filename);
  show_cnt = j - i;
  if (j - i > 64)
    {
      show_cnt = 64;
      msg ("Showing first differing %zu bytes.", show_cnt);
    }
  msg ("Data actually read:");
  hex_dump (ofs + i, read_data + i, show_cnt, true);
  msg ("Expected data:");
  hex_dump (ofs + i, expected_data + i, show_cnt, true);
  fail ("%zu bytes read starting at offset %zu in \"%s\" differ "
        "from expected", j - i, ofs, filename);
}
