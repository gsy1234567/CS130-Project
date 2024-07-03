#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <lib/kernel/fix_point.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

struct thread;
struct Priority_Donate_Info;
struct Priority_Donate_Receive_Info;
struct lock;

struct Priority_Donate_Receive_Info {
   struct list_elem elem;
   struct thread* receive_from;
   int priority;
   struct lock* lk;
};

bool priority_larger_func(const struct list_elem* lhs, const struct list_elem* rhs, void* aux);
bool priority_donate_receive_info_larger_func(const struct list_elem* lhs, const struct list_elem* rhs, void* aux);

struct Priority_Donate_Info {
   struct thread* donate_to;
   struct list receive_info;
};


/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Shared between thread.c and timer.c */
    int64_t wake_up;

    struct Priority_Donate_Info priority_donate_info;

    int32_t nice;
    fixed_t recent_cpu;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int any_thread_get_priority(const struct thread*);
void any_thread_set_priority(struct thread* , int);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

// Priority Donation Interface //

void priority_donate_info_init(struct Priority_Donate_Info*);

int get_donated_priority(const struct Priority_Donate_Info*);

/**
 * @brief 若`t->priority_donate_info.donate_to`不为空,则将`t`捐献出的优先级改为`t`的优先级,并更新`t`优先级的接受者的优先级.
 * 
*/
void update_priority(struct thread* t);

/**
 * @brief 1.根据`from`,`from`的优先级和`lk`构造`receive_info`,并将其存入`to->priority_donate_info.receive_info`中.
 *        2.将`from->priority_donate_info.donate_to`中.
 *        3.更新`to`的优先级.
*/
void donate_priority(struct Priority_Donate_Receive_Info* receive_info, struct thread* from, struct thread* to, struct lock* lk);

/**
 * @brief   1.找到所有通过`lk`将优先级捐献给`t`的线程,将这些线程的捐赠者改为无.
*/
void release_priority(struct thread* t, struct lock* lk);

int get_ready_list_highest_pri(void);

void mfqls_timer_callback(int64_t ticks);

#endif /* threads/thread.h */
