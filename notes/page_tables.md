# Page tables

See <https://pkuflyingpig.gitbook.io/pintos/appendix/reference-guide/page-table>

## Accessed and Dirty bits

For each page table entry, the 80x86 arquitecture maintains an **accessed bit** (set to 1 for
each read or write to the page) and a **dirty bit** (set to 1 for each write to the page).

The CPU never set this bits to zero but for **page replacement** purposes the OS may want to do so.

### Aliases

When two or more pages refer to the same frame, these multiple entries (aliases) are maintained
individualy. This means that the accessed and dirty bits are maintained for each alias.

## The 80x86 Page table

A page directory (PD) is allocated for every process, allowing each virtual space to be
independent. It consists of 1024 PDEs of 32-bit each.

Each PDE may point to a 4MB Page Table. A page table is arranged as 1024 32-bit PTEs, each
of which translates a 4KB virtual page to a 4KB physical page.

Together they can map a maximum of 4GB of virtual space.

Each PTE is formatted as follows:

```md
 31                                   12 11 9      6 5     2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            | AVL|    |D|A|   |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```

- Bit 0: The "present" bit. Page fault will occur if this bit is set to 0.
- Bit 1: The "writable" bit. This page is writable if bit 1 is set to 0. Write attempts will
  page fault.
- Bit 2: The "user" bit. When 0, only kernel-mode code can access this page.
- Bit 5: The "accessed" bit.
- Bit 6: The "dirty" bit.
- Bits 9-11: Available for use by the OS.
- Bits 12-31: The top 20 bits of the frame physical address. The rest 12 bits are set to 0.

> When a page fault occurs, the address attemped to be accessed is stored in the CR2 register.
>
> See <https://en.wikipedia.org/wiki/Control_register#CR2>

PDE share the same format, except for the physical address points to a Page Table instead of a
physical frame.

## Address translation

A 32-bit virtual address will use the following format to translate to a physical address.

```md
 31                  22 21                  12 11                   0
+----------------------+----------------------+----------------------+
| Page Directory Index |   Page Table Index   |    Page Offset       |
+----------------------+----------------------+----------------------+
             |                    |                     |
     _______/             _______/                _____/
    /                    /                       /
   /    Page Directory  /      Page Table       /    Data Page
  /     .____________. /     .____________.    /   .____________.
  |1,023|____________| |1,023|____________|    |   |____________|
  |1,022|____________| |1,022|____________|    |   |____________|
  |1,021|____________| |1,021|____________|    \__\|____________|
  |1,020|____________| |1,020|____________|       /|____________|
  |     |            | |     |            |        |            |
  |     |            | \____\|            |_       |            |
  |     |      .     |      /|      .     | \      |      .     |
  \____\|      .     |_      |      .     |  |     |      .     |
       /|      .     | \     |      .     |  |     |      .     |
        |      .     |  |    |      .     |  |     |      .     |
        |            |  |    |            |  |     |            |
        |____________|  |    |____________|  |     |____________|
       4|____________|  |   4|____________|  |     |____________|
       3|____________|  |   3|____________|  |     |____________|
       2|____________|  |   2|____________|  |     |____________|
       1|____________|  |   1|____________|  |     |____________|
       0|____________|  \__\0|____________|  \____\|____________|
                           /                      /
```
