#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*👤 project2/userprog
  Checks whether the pointer passed from the user process is valid
  (within user virtual address space)

   PHYS_BASE +----------------------------------+
             |            user stack            |
             |                 |                |
             |                 |                |
             |                 V                |
             |          grows downward          |
             |                                  |
             |                                  |
             |                                  |
             |                                  |
             |           grows upward           |
             |                 ^                |
             |                 |                |
             |                 |                |
             +----------------------------------+
             | uninitialized data segment (BSS) |
             +----------------------------------+
             |     initialized data segment     |
             +----------------------------------+
             |           code segment           |
  0x08048000 +----------------------------------+ STACK_BOTTOM
             |                                  |
             |                                  |
             |                                  |
             |                                  |
             |                                  |
           0 +----------------------------------+
*/
inline bool
is_valid_uaddr (void *addr)
{
  return addr != 0 && addr >= STACK_BOTTOM && addr < PHYS_BASE;
}

/* 👤 project2/userprog
   Gets the arguments from the stack and stores them in the arg array */
void
get_arguments (int *esp, int *arg, int count)
{
  int i;
  int* addr;

  for (i = 0; i < count; i++)
    {
      addr = esp + 1 + i;

      if (!is_valid_uaddr(addr))
        sys_exit(-1);

      arg[i] = *(int32_t *)(addr);
    }
}

/* 👤 project2/userprog
   Handler for system call interrupts */
static void
syscall_handler (struct intr_frame *f)
{
  if (!is_valid_uaddr (f->esp))
  {
    sys_exit (-1);
  }

  int argv[3]; // at most 3 arguments

  switch (*(int *)f->esp)
  {
    case SYS_HALT:
      break;

    case SYS_EXIT:
      get_arguments (f->esp, &argv[0], 1);
      sys_exit (argv[0]);
      break;

    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;

    case SYS_WRITE:
      get_arguments (f->esp, &argv[0], 3);
      if (!is_valid_uaddr ((void*) argv[1]))
        sys_exit (-1);

      f->eax = sys_write ((int) argv[0], (const void*) argv[1], (unsigned) argv[2]);
      break;

    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
  }
}

void
sys_halt (void)
{

}

/* 👤 project2/userprog
 Prints termination message when the kernel thread terminates.
*/
void
sys_exit (int status)
{
  struct thread *t = thread_current ();
  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

pid_t
sys_exec (const char *cmd_line)
{

}

int
sys_wait (pid_t pid)
{

}

bool
sys_create (const char *file, unsigned initial_size)
{

}

bool
sys_remove (const char *file)
{

}

int
sys_open (const char *file)
{

}

int
sys_filesize (int fd)
{

}

int
sys_read (int fd, void *buffer, unsigned size)
{

}

/*👤 project2/userprog
  Writes to the file descriptor fd from buffer
  fd: file descriptor
  buffer: buffer to write from
  size: number of bytes to write
  Returns the number of bytes written
*/
int
sys_write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }

  return -1;
}

void
sys_seek (int fd, unsigned position)
{

}

unsigned
sys_tell (int fd)
{

}

void
sys_close (int fd)
{

}
