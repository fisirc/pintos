#include <syscall.h>
#include "syscall-stub.h"
#include "../syscall-nr.h"

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER)                                \
        ({                                              \
          int retval;                                   \
          asm volatile                                  \
            ("push %[number]; int 0x30; add %%esp, 4"   \
               : "=a" (retval)                          \
               : [number] "i" (NUMBER)                  \
               : "memory");                             \
          retval;                                       \
        })

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0)                                          \
        ({                                                              \
          int retval;                                                   \
          asm volatile                                                  \
            ("push %[arg0]; push %[number]; int 0x30; add %%esp, 8"     \
               : "=a" (retval)                                          \
               : [number] "i" (NUMBER),                                 \
                 [arg0] "g" (ARG0)                                      \
               : "memory");                                             \
          retval;                                                       \
        })

/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1)                    \
        ({                                              \
          int retval;                                   \
          asm volatile                                  \
            ("push %[arg1]; push %[arg0]; "             \
             "push %[number]; int 0x30; add %%esp, 12"  \
               : "=a" (retval)                          \
               : [number] "i" (NUMBER),                 \
                 [arg0] "g" (ARG0),                     \
                 [arg1] "g" (ARG1)                      \
               : "memory");                             \
          retval;                                       \
        })

/* Invokes syscall NUMBER, passing arguments ARG0, ARG1, and
   ARG2, and returns the return value as an `int'. */
#define syscall3(NUMBER, ARG0, ARG1, ARG2)                      \
        ({                                                      \
          int retval;                                           \
          asm volatile                                          \
            ("push %[arg2]; push %[arg1]; push %[arg0]; "       \
             "push %[number]; int 0x30; add %%esp, 16"          \
               : "=a" (retval)                                  \
               : [number] "i" (NUMBER),                         \
                 [arg0] "g" (ARG0),                             \
                 [arg1] "g" (ARG1),                             \
                 [arg2] "g" (ARG2)                              \
               : "memory");                                     \
          retval;                                               \
        })

void
halt (void)
{
  syscall0 (SYS_halt);
  NOT_REACHED ();
}

void
exit (int status)
{
  syscall1 (SYS_exit, status);
  NOT_REACHED ();
}

pid_t
exec (const char *file)
{
  return (pid_t) syscall1 (SYS_exec, file);
}

int
join (pid_t pid)
{
  return syscall1 (SYS_join, pid);
}

bool
create (const char *file, unsigned initial_size)
{
  return syscall2 (SYS_create, file, initial_size);
}

bool
remove (const char *file)
{
  return syscall1 (SYS_remove, file);
}

int
open (const char *file)
{
  return syscall1 (SYS_open, file);
}

int
filesize (int fd)
{
  return syscall1 (SYS_filesize, fd);
}

int
read (int fd, void *buffer, unsigned size)
{
  return syscall3 (SYS_read, fd, buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  return syscall3 (SYS_write, fd, buffer, size);
}

void
seek (int fd, unsigned position)
{
  syscall2 (SYS_seek, fd, position);
}

unsigned
tell (int fd)
{
  return syscall1 (SYS_tell, fd);
}

void
close (int fd)
{
  syscall1 (SYS_close, fd);
}

bool
mmap (int fd, void *addr, unsigned length)
{
  return syscall3 (SYS_mmap, fd, addr, length);
}

bool
munmap (void *addr, unsigned length)
{
  return syscall2 (SYS_munmap, addr, length);
}

bool
chdir (const char *dir)
{
  return syscall1 (SYS_chdir, dir);
}

bool
mkdir (const char *dir)
{
  return syscall1 (SYS_mkdir, dir);
}

void
lsdir (void)
{
  syscall0 (SYS_lsdir);
}

