/* ls.c

   Lists the contents of the directory or directories named on
   the command line, or of the current directory if none are
   named.

   By default, only the name of each file is printed.  If "-l" is
   given as the first argument, the type and size of each file is
   also printed. */

#include <syscall.h>
#include <stdio.h>
#include <string.h>

static void
list_dir (const char *dir, bool verbose)
{
  int dir_fd = open (dir);
  if (dir_fd == -1)
    {
      printf ("%s: not found\n", dir);
      return;
    }

  if (isdir (dir_fd))
    {
      char name[READDIR_MAX_LEN];
      printf ("%s:\n", dir);
      while (readdir (dir_fd, name))
        {
          printf ("%s", name);
          if (verbose)
            {
              char full_name[128];
              int entry_fd;

              if (strcmp (dir, "."))
                snprintf (full_name, sizeof full_name, "%s/%s", dir, name);
              else
                {
                  /* This is a special case for implementations
                     that don't fully understand . and .. */
                  strlcpy (full_name, name, sizeof full_name);
                }
              entry_fd = open (full_name);

              printf (": ");
              if (entry_fd != -1)
                {
                  if (isdir (entry_fd))
                    printf ("directory");
                  else
                    printf ("%d-byte file", filesize (entry_fd));
                }
              else
                printf ("open failed");
              close (entry_fd);
            }
          printf ("\n");
        }
    }
  else
    printf ("%s: not a directory\n", dir);
  close (dir_fd);
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  if (argc > 1 && !strcmp (argv[1], "-l"))
    {
      verbose = true;
      argv++;
      argc--;
    }

  if (argc <= 1)
    list_dir (".", verbose);
  else
    {
      int i;
      for (i = 1; i < argc; i++)
        list_dir (argv[i], verbose);
    }
  return 0;
}
