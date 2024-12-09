# Virtual memory layout

For a maximum of 4GB **virtual** address space, pintos defines two regions:

- 3GB for user space, ranging from 0 up to `PHYS_BASE`
  (defined in [loader.h](./../src/threads/loader.h))

- 1GB for kernel space, ranging from `PHYS_BASE` up to 4GB

The user virtual memory is per-process.

- When the kernel performs a context switch, it also switches user virtual address space by
  using a different page directory (PD)
  (See pagedir_activate() in [userprog/pagedir.c](../src/userprog/pagedir.c)).
- The `struct thread` contains a pointer to the process' Page Directory. This way a user program
  can only access its own virtual memory. An attempt to access kernel virtual memory will result
  in a `page_fault()` and process will be terminated.

Kernel virtual memory is global.

- It's always mapped the same way regardless of what process the thread is running.
- In Pintos kernel virtual memory is mapped one-to-one to physical memory, with an offset of
  `PHYS_BASE`. That is, a kernel virtual address of `PHYS_BASE` will map to physical address 0,
  `PHYS_BASE + n` will map to physical address `n`, etc.

## Process memory layout

Conceptually a process can lay out his memory however it wants. But in practice, it follows a
standard layout.

```txt
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
  0x08048000 +----------------------------------+
             |                                  |
             |                                  |
             |                                  |
             |                                  |
             |                                  |
           0 +----------------------------------+
```

In original pintos the stack is fixed-size, but in project 3 it will be allowed to grow.
