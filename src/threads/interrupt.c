#include "interrupt.h"
#include <inttypes.h>
#include <stdint.h>
#include "intr-stubs.h"
#include "debug.h"
#include "io.h"
#include "lib.h"
#include "mmu.h"
#include "thread.h"
#include "timer.h"

enum if_level
intr_get_level (void)
{
  uint32_t flags;

  asm ("pushfl; popl %0" : "=g" (flags));

  return flags & (1 << 9) ? IF_ON : IF_OFF;
}

enum if_level
intr_set_level (enum if_level level)
{
  enum if_level old_level = intr_get_level ();
  if (level == IF_ON)
    intr_enable ();
  else
    intr_disable ();
  return old_level;
}

enum if_level
intr_enable (void)
{
  enum if_level old_level = intr_get_level ();
  asm volatile ("sti");
  return old_level;
}

enum if_level
intr_disable (void)
{
  enum if_level old_level = intr_get_level ();
  asm volatile ("cli");
  return old_level;
}

static void
pic_init (void)
{
  /* Every PC has two 8259A Programmable Interrupt Controller
     (PIC) chips.  One is a "master" accessible at ports 0x20 and
     0x21.  The other is a "slave" cascaded onto the master's IRQ
     2 line and accessible at ports 0xa0 and 0xa1.  Accesses to
     port 0x20 set the A0 line to 0 and accesses to 0x21 set the
     A1 line to 1.  The situation is similar for the slave PIC.
     Refer to the 8259A datasheet for details.

     By default, interrupts 0...15 delivered by the PICs will go
     to interrupt vectors 0...15.  Unfortunately, those vectors
     are also used for CPU traps and exceptions.  We reprogram
     the PICs so that interrupts 0...15 are delivered to
     interrupt vectors 32...47 instead. */

  /* Mask all interrupts on both PICs. */
  outb (0x21, 0xff);
  outb (0xa1, 0xff);

  /* Initialize master. */
  outb (0x20, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
  outb (0x21, 0x20); /* ICW2: line IR0...7 -> irq 0x20...0x27. */
  outb (0x21, 0x04); /* ICW3: slave PIC on line IR2. */
  outb (0x21, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

  /* Initialize slave. */
  outb (0xa0, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
  outb (0xa1, 0x28); /* ICW2: line IR0...7 -> irq 0x28...0x2f. */
  outb (0xa1, 0x02); /* ICW3: slave ID is 2. */
  outb (0xa1, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */

  /* Unmask all interrupts. */
  outb (0x21, 0x00);
  outb (0xa1, 0x00);
}

/* Sends an end-of-interrupt signal to the PIC for the given IRQ.
   If we don't acknowledge the IRQ, we'll never get it again, so
   this is important.  */
static void
pic_eoi (int irq)
{
  /* FIXME?  The Linux code is much more complicated. */
  ASSERT (irq >= 0x20 && irq < 0x30);
  outb (0x20, 0x20);
  if (irq >= 0x28)
    outb (0xa0, 0x20);
}

#define INTR_CNT 256

static uint64_t idt[INTR_CNT];
static intr_handler_func *intr_handlers[INTR_CNT];
static const char *intr_names[INTR_CNT];

void intr_handler (struct intr_frame *args);

static bool intr_in_progress;
static bool yield_on_return;

const char *
intr_name (int vec)
{
  if (vec < 0 || vec >= INTR_CNT || intr_names[vec] == NULL)
    return "unknown";
  else
    return intr_names[vec];
}

void
intr_handler (struct intr_frame *args)
{
  bool external = args->vec_no >= 0x20 && args->vec_no < 0x30;
  if (external)
    {
      ASSERT (intr_get_level () == IF_OFF);
      ASSERT (!intr_context ());
      intr_in_progress = true;
      yield_on_return = false;
    }

  intr_handlers[args->vec_no] (args);

  if (external)
    {
      ASSERT (intr_get_level () == IF_OFF);
      ASSERT (intr_context ());
      intr_in_progress = false;
      pic_eoi (args->vec_no);

      if (yield_on_return)
        thread_yield ();
    }
}

bool
intr_context (void)
{
  return intr_in_progress;
}

void
intr_yield_on_return (void)
{
  ASSERT (intr_context ());
  yield_on_return = true;
}

intr_handler_func intr_panic NO_RETURN;
intr_handler_func intr_kill NO_RETURN;

static uint64_t
make_gate (void (*target) (void), int dpl, enum seg_type type)
{
  uint32_t offset = (uint32_t) target;
  uint32_t e0 = ((offset & 0xffff)            /* Offset 15:0. */
                 | (SEL_KCSEG << 16));        /* Target code segment. */
  uint32_t e1 = ((offset & 0xffff0000)        /* Offset 31:16. */
                 | (1 << 15)                  /* Present. */
                 | ((uint32_t) dpl << 13)     /* Descriptor privilege. */
                 | (SYS_SYSTEM << 12)         /* System. */
                 | ((uint32_t) type << 8));   /* Gate type. */
  return e0 | ((uint64_t) e1 << 32);
}

static uint64_t
make_intr_gate (void (*target) (void), int dpl)
{
  return make_gate (target, dpl, TYPE_INT_32);
}

static uint64_t
make_trap_gate (void (*target) (void), int dpl)
{
  return make_gate (target, dpl, TYPE_TRAP_32);
}

void
intr_register (uint8_t vec_no, int dpl, enum if_level level,
               intr_handler_func *handler,
               const char *name)
{
  /* Interrupts generated by external hardware (0x20 <= VEC_NO <=
     0x2f) should specify IF_OFF for LEVEL.  Otherwise a timer
     interrupt could cause a task switch during interrupt
     handling.  Most other interrupts can and should be handled
     with interrupts enabled. */
  ASSERT (vec_no < 0x20 || vec_no > 0x2f || level == IF_OFF);

  if (level == IF_ON)
    idt[vec_no] = make_trap_gate (intr_stubs[vec_no], dpl);
  else
    idt[vec_no] = make_intr_gate (intr_stubs[vec_no], dpl);
  intr_handlers[vec_no] = handler;
  intr_names[vec_no] = name;
}

void
intr_init (void)
{
  uint64_t idtr_operand;
  int i;

  pic_init ();

  /* Install default handlers. */
  for (i = 0; i < 256; i++)
    intr_register (i, 0, IF_OFF, intr_panic, NULL);

  /* Most exceptions require ring 0.
     Exceptions 3, 4, and 5 can be caused by ring 3 directly. */
  intr_register (0, 0, IF_ON, intr_kill, "#DE Divide Error");
  intr_register (1, 0, IF_ON, intr_kill, "#DB Debug Exception");
  intr_register (2, 0, IF_ON, intr_panic, "NMI Interrupt");
  intr_register (3, 3, IF_ON, intr_kill, "#BP Breakpoint Exception");
  intr_register (4, 3, IF_ON, intr_kill, "#OF Overflow Exception");
  intr_register (5, 3, IF_ON, intr_kill, "#BR BOUND Range Exceeded Exception");
  intr_register (6, 0, IF_ON, intr_kill, "#UD Invalid Opcode Exception");
  intr_register (7, 0, IF_ON, intr_kill, "#NM Device Not Available Exception");
  intr_register (8, 0, IF_ON, intr_panic, "#DF Double Fault Exception");
  intr_register (9, 0, IF_ON, intr_panic, "Coprocessor Segment Overrun");
  intr_register (10, 0, IF_ON, intr_panic, "#TS Invalid TSS Exception");
  intr_register (11, 0, IF_ON, intr_kill, "#NP Segment Not Present");
  intr_register (12, 0, IF_ON, intr_kill, "#SS Stack Fault Exception");
  intr_register (13, 0, IF_ON, intr_kill, "#GP General Protection Exception");
  intr_register (16, 0, IF_ON, intr_kill, "#MF x87 FPU Floating-Point Error");
  intr_register (17, 0, IF_ON, intr_panic, "#AC Alignment Check Exception");
  intr_register (18, 0, IF_ON, intr_panic, "#MC Machine-Check Exception");
  intr_register (19, 0, IF_ON, intr_kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register (14, 0, IF_OFF, intr_kill, "#PF Page-Fault Exception");

  idtr_operand = make_dtr_operand (sizeof idt - 1, idt);
  asm volatile ("lidt %0" :: "m" (idtr_operand));
}

static void
dump_intr_frame (struct intr_frame *f)
{
  uint32_t cr2, ss;
  asm ("movl %%cr2, %0" : "=r" (cr2));
  asm ("movl %%ss, %0" : "=r" (ss));

  printk ("Interrupt %#04x (%s) at eip=%p\n",
          f->vec_no, intr_name (f->vec_no), f->eip);
  printk (" cr2=%08"PRIx32" error=%08"PRIx32"\n", cr2, f->error_code);
  printk (" eax=%08"PRIx32" ebx=%08"PRIx32" ecx=%08"PRIx32" edx=%08"PRIx32"\n",
          f->eax, f->ebx, f->ecx, f->edx);
  printk (" esi=%08"PRIx32" edi=%08"PRIx32" esp=%08"PRIx32" ebp=%08"PRIx32"\n",
          f->esi, f->edi, (uint32_t) f->esp, f->ebp);
  printk (" cs=%04"PRIx16" ds=%04"PRIx16" es=%04"PRIx16" ss=%04"PRIx16"\n",
          f->cs, f->ds, f->es, f->cs != SEL_KCSEG ? f->ss : ss);
}

void
intr_panic (struct intr_frame *regs)
{
  dump_intr_frame (regs);
  panic ("Panic!");
}

void
intr_kill (struct intr_frame *f)
{
  switch (f->cs)
    {
    case SEL_UCSEG:
      printk ("%s: dying due to interrupt %#04x (%s).\n",
              thread_current ()->name, f->vec_no, intr_name (f->vec_no));
      thread_exit ();

    case SEL_KCSEG:
      printk ("Kernel bug - unexpected interrupt in kernel context\n");
      intr_panic (f);

    default:
      printk ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}


