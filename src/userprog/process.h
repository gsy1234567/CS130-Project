#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process_ret_frame
  {
    struct list_elem elem;
    tid_t tid;    /* Record the thread id of the children thread. */
    int exit_code;/* Record the exit code of yhr children thread.*/
    bool finish;
  };

#endif /* userprog/process.h */
