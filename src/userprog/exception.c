#include "exception.h"
#include <inttypes.h>
#include "lib.h"
#include "gdt.h"
#include "interrupt.h"
#include "thread.h"

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, but
   we don't implement signals.  Instead, we'll make them simply
   kill the user process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3] section 5.14 for a description of each of
   these exceptions. */
void
exception_init (void)
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f)
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printk ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (thread_current ()),
              f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit ();

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel");

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printk ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros above, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3] section 5.14, which is pages 5-46 to 5-49. */
static void
page_fault (struct intr_frame *f)
{
  bool not_present, write, user;
  uint32_t fault_addr;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* Obtain faulting address.

     (The faulting address is not necesarily the address of the
     instruction that caused the fault--that's in F's eip
     member.  Rather, it's the linear address that was accessed
     to cause the fault, which is probably an address of data,
     not code.) */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printk ("Page fault on address %08"PRIx32": %s %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}

