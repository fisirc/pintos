#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
// 👤 project2/userprog
int process_parse_cmd (char *cmd, char **argv);
void process_init_stack_args (char **argv, int argc, void **esp);

#endif /* userprog/process.h */
