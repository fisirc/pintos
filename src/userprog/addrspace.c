#include "addrspace.h"
#include <inttypes.h>
#include "debug.h"
#include "file.h"
#include "filesys.h"
#include "lib.h"
#include "mmu.h"
#include "malloc.h"
#include "paging.h"
#include "palloc.h"

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification more-or-less verbatim. */

/* ELF types. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

#define PE32Wx PRIx32
#define PE32Ax PRIx32
#define PE32Ox PRIx32
#define PE32Hx PRIx16

/* Executable header.
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

/* Program header.
   There are e_phnum of these, starting at file offset e_phoff. */
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

/* Values for p_type. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

#define LOAD_ERROR(MSG)                                         \
        do {                                                    \
                printk ("addrspace_load: %s: ", filename);      \
                printk MSG;                                     \
                printk ("\n");                                  \
                goto error;                                     \
        } while (0)

static bool
load_segment (struct addrspace *as, struct file *file,
              const struct Elf32_Phdr *phdr)
{
  uintptr_t start, end;
  uint8_t *upage;
  off_t filesz_left;

  ASSERT (as != NULL);
  ASSERT (file != NULL);
  ASSERT (phdr != NULL);
  ASSERT (phdr->p_type == PT_LOAD);

  /* p_offset and p_vaddr must be congruent modulo NBPG. */
  if (phdr->p_offset % NBPG != phdr->p_vaddr % NBPG)
    {
      printk ("%#08"PE32Ox" and %#08"PE32Ax" not congruent modulo %#x\n",
              phdr->p_offset, phdr->p_vaddr, (unsigned) NBPG);
      return false;
    }

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    {
      printk ("p_memsz (%08"PE32Wx") < p_filesz (%08"PE32Wx")\n",
              phdr->p_memsz, phdr->p_filesz);
      return false;
    }

  /* Validate virtual memory region to be mapped. */
  start = PGROUNDDOWN (phdr->p_vaddr);
  end = PGROUNDUP (phdr->p_vaddr + phdr->p_memsz);
  if (start >= PHYS_BASE || end >= PHYS_BASE || end < start)
    {
      printk ("bad virtual region %08lx...%08lx\n",
              (unsigned long) start, (unsigned long) end);
      return false;
    }

  filesz_left = phdr->p_filesz + (phdr->p_vaddr - start);
  file_seek (file, ROUND_DOWN (phdr->p_offset, NBPG));
  for (upage = (uint8_t *) start; upage < (uint8_t *) end; upage += NBPG)
    {
      size_t read_bytes = filesz_left >= NBPG ? NBPG : filesz_left;
      size_t zero_bytes = NBPG - read_bytes;
      uint8_t *kpage = palloc_get (0);
      if (kpage == NULL)
        return false;

      if (file_read (file, kpage, read_bytes) != (int) read_bytes)
        return false;
      memset (kpage + read_bytes, 0, zero_bytes);
      filesz_left -= read_bytes;

      if (pagedir_get_page (as->pagedir, upage))
        {
          palloc_free (kpage);
          return false;
        }
      pagedir_set_page (as->pagedir, upage, kpage, true);
    }

  return true;
}

bool
addrspace_load (struct addrspace *as, const char *filename)
{
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  as->pagedir = pagedir_create ();

  file = filesys_open (filename);
  if (file == NULL)
    LOAD_ERROR (("open failed"));

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr)
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
  printk ("e_phnum=%d\n", ehdr.e_phnum);
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      file_seek (file, file_ofs);
      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        LOAD_ERROR (("error reading program header"));
      printk ("%x: %08x, %08x %08x %08x %05x %05x\n",
              file_tell (file),
              phdr.p_type,
              phdr.p_offset, phdr.p_vaddr, phdr.p_paddr,
              phdr.p_filesz, phdr.p_memsz);
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK: /* Stack segment. */
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
          if (!load_segment (as, file, &phdr))
            goto error;
          break;
        }
    }
  success = true;

 error:
  if (file != NULL)
    file_close (file);
  if (!success)
    addrspace_destroy (as);
  return success;
}

void
addrspace_destroy (struct addrspace *as)
{
  if (as != NULL && as->pagedir != NULL)
    pagedir_destroy (as->pagedir);
}
