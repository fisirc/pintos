#include "addrspace.h"
#include <inttypes.h>
#include "tss.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/debug.h"
#include "lib/lib.h"
#include "threads/init.h"
#include "threads/mmu.h"
#include "threads/paging.h"
#include "threads/palloc.h"
#include "threads/thread.h"

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printk(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool load_segment (struct thread *, struct file *,
                          const struct Elf32_Phdr *);
static bool setup_stack (struct thread *);

/* Aborts loading an executable, with an error message. */
#define LOAD_ERROR(MSG)                                         \
        do {                                                    \
                printk ("addrspace_load: %s: ", filename);      \
                printk MSG;                                     \
                printk ("\n");                                  \
                goto done;                                     \
        } while (0)

/* Loads an ELF executable from FILENAME into T,
   and stores the executable's entry point into *START.
   Returns true if successful, false otherwise. */
bool
addrspace_load (struct thread *t, const char *filename, void (**start) (void))
{
  struct Elf32_Ehdr ehdr;
  struct file file;
  bool file_open = false;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    LOAD_ERROR (("page directory allocation failed"));

  /* Open executable file. */
  file_open = filesys_open (filename, &file);
  if (!file_open)
    LOAD_ERROR (("open failed"));

  /* Read and verify executable header. */
  if (file_read (&file, &ehdr, sizeof ehdr) != sizeof ehdr)
    LOAD_ERROR (("error reading executable header"));
  if (memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7) != 0)
    LOAD_ERROR (("file is not ELF"));
  if (ehdr.e_type != 2)
    LOAD_ERROR (("ELF file is not an executable"));
  if (ehdr.e_machine != 3)
    LOAD_ERROR (("ELF executable is not x86"));
  if (ehdr.e_version != 1)
    LOAD_ERROR (("ELF executable hasunknown version %d",
                 (int) ehdr.e_version));
  if (ehdr.e_phentsize != sizeof (struct Elf32_Phdr))
    LOAD_ERROR (("bad ELF program header size"));
  if (ehdr.e_phnum > 1024)
    LOAD_ERROR (("too many ELF program headers"));

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      file_seek (&file, file_ofs);
      if (file_read (&file, &phdr, sizeof phdr) != sizeof phdr)
        LOAD_ERROR (("error reading program header"));
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          /* Reject the executable. */
          LOAD_ERROR (("unsupported ELF segment type %d\n", phdr.p_type));
          break;
        default:
          printk ("unknown ELF segment type %08x\n", phdr.p_type);
          break;
        case PT_LOAD:
          if (!load_segment (t, &file, &phdr))
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (t))
    goto done;

  /* Start address. */
  *start = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not.
     We can distinguish based on `success'. */
  if (file_open)
    file_close (&file);
  if (!success)
    addrspace_destroy (t);
  return success;
}

/* Destroys the user address space in T and frees all of its
   resources. */
void
addrspace_destroy (struct thread *t)
{
  if (t->pagedir != NULL)
    {
      pagedir_destroy (t->pagedir);
      t->pagedir = NULL;
    }
}

/* Sets up the CPU for running user code in thread T, if any. */
void
addrspace_activate (struct thread *t)
{
  ASSERT (t != NULL);

  /* Activate T's page tables. */
  pagedir_activate (t->pagedir);

  /* Set T's kernel stack for use in processing interrupts. */
  tss_set_esp0 ((uint8_t *) t + PGSIZE);
}

/* addrspace_load() helpers. */

static bool install_page (struct thread *, void *upage, void *kpage);

/* Loads the segment described by PHDR from FILE into thread T's
   user address space.  Return true if successful, false
   otherwise. */
static bool
load_segment (struct thread *t, struct file *file,
              const struct Elf32_Phdr *phdr)
{
  void *start, *end;  /* Page-rounded segment start and end. */
  uint8_t *upage;     /* Iterator from start to end. */
  off_t filesz_left;  /* Bytes left of file data (as opposed to
                         zero-initialized bytes). */

  ASSERT (t != NULL);
  ASSERT (file != NULL);
  ASSERT (phdr != NULL);
  ASSERT (phdr->p_type == PT_LOAD);

  /* [ELF1] 2-2 says that p_offset and p_vaddr must be congruent
     modulo PGSIZE. */
  if (phdr->p_offset % PGSIZE != phdr->p_vaddr % PGSIZE)
    {
      printk ("%#08"PE32Ox" and %#08"PE32Ax" not congruent modulo %#x\n",
              phdr->p_offset, phdr->p_vaddr, (unsigned) PGSIZE);
      return false;
    }

  /* [ELF1] 2-3 says that p_memsz must be at least as big as
     p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    {
      printk ("p_memsz (%08"PE32Wx") < p_filesz (%08"PE32Wx")\n",
              phdr->p_memsz, phdr->p_filesz);
      return false;
    }

  /* Validate virtual memory region to be mapped.
     The region must both start and end within the user address
     space range starting at 0 and ending at PHYS_BASE (typically
     3 GB == 0xc0000000). */
  start = pg_round_down ((void *) phdr->p_vaddr);
  end = pg_round_up ((void *) (phdr->p_vaddr + phdr->p_memsz));
  if (start >= PHYS_BASE || end >= PHYS_BASE || end < start)
    {
      printk ("bad virtual region %08lx...%08lx\n",
              (unsigned long) start, (unsigned long) end);
      return false;
    }

  /* Load the segment page-by-page into memory. */
  filesz_left = phdr->p_filesz + (phdr->p_vaddr & PGMASK);
  file_seek (file, ROUND_DOWN (phdr->p_offset, PGSIZE));
  for (upage = start; upage < (uint8_t *) end; upage += PGSIZE)
    {
      /* We want to read min(PGSIZE, filesz_left) bytes from the
         file into the page and zero the rest. */
      size_t read_bytes = filesz_left >= PGSIZE ? PGSIZE : filesz_left;
      size_t zero_bytes = PGSIZE - read_bytes;
      uint8_t *kpage = palloc_get (0);
      if (kpage == NULL)
        return false;

      /* Do the reading and zeroing. */
      if (file_read (file, kpage, read_bytes) != (int) read_bytes)
        {
          palloc_free (kpage);
          return false;
        }
      memset (kpage + read_bytes, 0, zero_bytes);
      filesz_left -= read_bytes;

      /* Add the page to the process's address space. */
      if (!install_page (t, upage, kpage))
        {
          palloc_free (kpage);
          return false;
        }
    }

  return true;
}

/* Create a minimal stack for T by mapping a zeroed page at the
   top of user virtual memory. */
static bool
setup_stack (struct thread *t)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get (PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (t, ((uint8_t *) PHYS_BASE) - PGSIZE, kpage);
      if (!success)
        palloc_free (kpage);
    }
  else
    printk ("failed to allocate process stack\n");

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to T's page tables.  Fails if UPAGE is
   already mapped or if memory allocation fails. */
static bool
install_page (struct thread *t, void *upage, void *kpage)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, true));
}
