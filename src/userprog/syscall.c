#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
struct lock file_lock;
int read_count;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
  read_count = 0;
}

/*👤 project2/userprog

   Checks whether a given pointer has a valid user space address

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
bool
is_valid_uaddr (void *addr)
{
  return addr != 0 && addr >= STACK_BOTTOM && addr < PHYS_BASE;
}

/* 👤 project2/userprog
   Gets the [count] syscall arguments from the stack and stores their
   references into [arg]
*/
void
get_syscall_args (int *esp, int *arg, int count)
{
  int i;
  for (i = 0; i < count; i++)
  {
    if (!is_valid_uaddr (esp + 1 + i)) { sys_exit (-1); }
    arg[i] = *(esp + 1 + i);
  }
}

/* 👤 project2/userprog
   Handler for *user* system call interrupts
*/
static void
syscall_handler (struct intr_frame *f)
{
  if (!is_valid_uaddr (f->esp))
    sys_exit (-1);

  thread_current()->esp = f->esp; // 🧠 project3/vm: save the esp for page fault handling
  int argv[3]; // at most 3 arguments

  // The system call number is stored in the first argument
  switch (*(int *)f->esp)
  {
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_EXIT:
      get_syscall_args (f->esp, &argv[0], 1);
      sys_exit (argv[0]);
      break;
    case SYS_EXEC:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_exec (argv[0]);
      break;
    case SYS_WAIT:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_wait (argv[0]);
      break;
    case SYS_CREATE:
      get_syscall_args (f->esp, &argv[0], 2);
      f->eax = sys_create ((const char *) argv[0], (const char *) argv[1]);
      break;
    case SYS_REMOVE:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_remove (argv[0]);
      break;
    case SYS_OPEN:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_open (argv[0]);
      break;
    case SYS_FILESIZE:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_filesize (argv[0]);
      break;
    case SYS_READ:
      get_syscall_args (f->esp, &argv[0], 3);
      f->eax = sys_read (argv[0], argv[1], argv[2]);
      break;
    case SYS_WRITE:
      get_syscall_args (f->esp, &argv[0], 3);
      if (!is_valid_uaddr ((void*) argv[1]))
        sys_exit (-1);
      f->eax = sys_write ((int) argv[0], (const void*) argv[1], (unsigned) argv[2]);
      break;
    case SYS_SEEK:
      get_syscall_args (f->esp, &argv[0], 2);
      sys_seek (argv[0], argv[1]);
      break;
    case SYS_TELL:
      get_syscall_args (f->esp, &argv[0], 1);
      f->eax = sys_tell (argv[0]);
      break;
    case SYS_CLOSE:
      get_syscall_args (f->esp, &argv[0], 1);
      sys_close (argv[0]);
      break;
  }
}

/* 👤 project2/userprog
 Shuts down the system
*/
void
sys_halt (void)
{
  shutdown_power_off ();
}

/* 👤 project2/userprog
 Prints termination message when the kernel thread terminates
*/
void
sys_exit (int status)
{
  struct thread *t = thread_current ();
  t->pcb->exit_code = status;
  if (!t->pcb->has_loaded)
    sema_up (&(t->pcb->sema_load));

  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

/* 👤 project2/userprog
  Executes the instruction passed through cmd_line using synchronization
  Waits for the parent process until the new process's PID is returned.
  Returns '-1' if the program cannot be 'loaded' or 'run'.
*/
pid_t
sys_exec (const char *cmd_line)
{
  pid_t pid = process_execute (cmd_line);
  struct pcb *child_pcb = thread_get_child (pid)->pcb;
  if (pid == -1 || !child_pcb->has_loaded)
    return -1;

  return pid;
}

/* 👤 project2/userprog
  Waits for the child process (pid) terminates
*/
int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

/* 👤 project2/userprog
  Creates a file with the given name and initial size
*/
bool
sys_create (const char *file, unsigned initial_size)
{
  if (file == NULL || !is_valid_uaddr (file))
    sys_exit (-1);

  return filesys_create (file, initial_size);
}

/* 👤 project2/userprog
  Removes the file with the given name, even if it's open.
  👁️: deleting an open file does not close the file
*/
bool
sys_remove (const char *file)
{
  if (file == NULL || !is_valid_uaddr (file))
    sys_exit (-1);

  return filesys_remove (file);
}

/* 👤 project2/userprog
   Opens the file with the given name
*/
int
sys_open (const char *file)
{
  struct file *file_;
  struct thread *t = thread_current ();
  int fd_count = t->pcb->fd_count;

  lock_acquire (&file_lock);

  if (file == NULL || !is_valid_uaddr (file))
    {
      lock_release (&file_lock); // 🧠 project3/vm: release the lock before exiting
      sys_exit (-1);
    }

  file_ = filesys_open (file);
  if (file_ == NULL)
    {
      lock_release (&file_lock); // 🧠 project3/vm: release the lock before exiting
      return -1;
    }

  if (thread_current ()->pcb->exec_file && (strcmp (thread_current ()->name, file) == 0)) 
    file_deny_write (file_);

  t->pcb->fd_table[t->pcb->fd_count++] = file_;
  lock_release (&file_lock);

  return fd_count;
}

/* 👤 project2/userprog
   Returns the size of the file managed by FD in bytes
*/
int
sys_filesize (int fd)
{
  struct thread *t = thread_current ();
  struct file *file = t->pcb->fd_table[fd];

  if (file == NULL)
    return -1;

  return file_length (file);
}

/* 👤 project2/userprog
   Reads SIZE amount of bytes from the file descriptor into the buffer,
   and returns the bytes actually read
*/
int
sys_read (int fd, void *buffer, unsigned size)
{
  if (!is_valid_uaddr (buffer))
    sys_exit (-1);

  int fd_count = thread_current ()->pcb->fd_count;
  int bytes_read;
  struct file *file = thread_current ()->pcb->fd_table[fd];

  if (file == NULL || fd < 0 || fd > fd_count)
    sys_exit (-1);

  lock_acquire (&file_lock);
  bytes_read = file_read (file, buffer, size);
  lock_release (&file_lock);

  return bytes_read;
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
  int fd_count = thread_current ()->pcb->fd_count;
  if (fd >= fd_count || fd < 1)
    {
      sys_exit (-1);
    }
  else if (fd == 1)
    {
      lock_acquire (&file_lock);
      putbuf(buffer, size);
      lock_release (&file_lock);
      return size;
    }
  else
    {
      int bytes_written;
      struct file *file = thread_current ()->pcb->fd_table[fd];

      if (file == NULL)
        sys_exit (-1);

      lock_acquire (&file_lock);
      bytes_written = file_write (file, buffer, size);
      lock_release (&file_lock);

      return bytes_written;
    }

  return -1;
}

/* 👤 project2/userprog
 Changes the FD current cursor position
*/
void
sys_seek (int fd, unsigned position)
{
  struct file *file;

  file = thread_current ()->pcb->fd_table[fd];
  if (file != NULL)
    file_seek (file, position);
}

/* 👤 project2/userprog
 Returns the position of the FD current cursor
*/
unsigned
sys_tell (int fd)
{
  struct file *file;

  file = thread_current ()->pcb->fd_table[fd];
  if (file == NULL)
    return -1;

  return file_tell (file);
}

/* 👤 project2/userprog
 Closes the file descriptor FD
*/
void
sys_close (int fd)
{
  struct file *file;
  struct thread *t = thread_current ();
  int i;

  if (fd >= t->pcb->fd_count || fd < 2)
    sys_exit (-1);

  file = t->pcb->fd_table[fd];
  if (file == NULL)
    return;

  file_close (file);
  t->pcb->fd_table[fd] = NULL;

  for (i = fd; i < t->pcb->fd_count; i++)
    t->pcb->fd_table[i] = t->pcb->fd_table[i + 1];

  t->pcb->fd_count--;
}
