# Pintos

## Resources

- Latest CS212 course reference: <https://www.scs.stanford.edu/24wi-cs212/pintos/pintos.html>
- Peking University reference: <https://pkuflyingpig.gitbook.io/pintos>
- Kernel newbies: <https://kernelnewbies.org/>

## Requirements

```sh
sudo apt install build-essential qemu-system-x86
```

## System setup

There's an `env` file we wrote file that provides you several utilities that you can call
directly from your terminal:

```bash
source env
```

For docker users:

```bash
docker compose up --build
```

In another terminal, run:

```bash
docker compose exec pintos bash
```

```bash
root@pwn13:~# cd pintos/src/threads/
root@pwn13::~/pintos/src/threads# make
root@pwn13::~/pintos/src/threads# cd build
root@pwn13::~/pintos/src/threads/build# pintos --qemu --
```

Pintos has succesfully booted! :)

Run multiple alarm:

```bash
root@pwn13::~/pintos/src/threads/build# pintos --qemu -- run alarm-multiple
```

## ️🔎 Debugging

Beyond writing printf all over the place, it's highly recommended to open a GDB session for
debugging.

The better way to debug a test is to directly set a breakpoint in the test function:

```console
(gdb) debugpintos
(gdb) b test_alarm_multiple
(gdb) c
```

### Common debugging commands

```console
dumplist &all_list thread allelem
dumplist &all_list thread allelem
dumplist &sleep_list thread allelem
```

## File structure

Pintos entry point is located in [`src/threads/init.c#main`](./src/threads/init.c).

- `threads/`

  Source code for the base kernel, which you will modify starting in project 1.

- `userprog/`

  Source code for the user program loader, which you will modify starting with project 2.

- `vm/`

  An almost empty directory. You will implement virtual memory here in project 3.

- `filesys/`

  Source code for a basic file system. You will use this file system starting with project 2,
  but you will not modify it until project 4.

- `devices/`

  Source code for I/O device interfacing: keyboard, timer, disk, etc. You will modify the
  timer implementation in project 1. Otherwise you should have no need to change this code.

- `lib/`

  An implementation of a subset of the standard C library. The code in this directory is
  compiled into both the Pintos kernel and, starting from project 2, user programs that run
  under it. In both kernel code and user programs, headers in this directory can be included
  using the #include <...> notation. You should have little need to modify this code.

- `lib/kernel/`

  Parts of the C library that are included only in the Pintos kernel. This also includes
  implementations of some data types that you are free to use in your kernel code: bitmaps,
  doubly linked lists, and hash tables. In the kernel, headers in this directory can be included
  using the #include <...> notation.

- `lib/user/`

  Parts of the C library that are included only in Pintos user programs. In user programs,
  headers in this directory can be included using the #include <...> notation.

- `tests/`

  Tests for each project. You can modify this code if it helps you test your submission, but we will replace it with the originals before we run the tests.

- `examples/`

  Example user programs for use starting with project 2.

- `misc/, utils/`

  These files may come in handy if you decide to try working with Pintos on your own machine. Otherwise, you can ignore them.
