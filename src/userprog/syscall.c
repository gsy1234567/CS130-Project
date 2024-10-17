#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "devices/shutdown.h"

// User addr checker

/**
 * \brief Reads a byte at user virtual address `uaddr`.
 * \return The byte value if successful, -1 otherwise.
*/
static int get_user(const uint8_t* uaddr);

/**
 * \brief Writes `byte` to user address `udst`.
 * \return True if successful, false if a segfault occurred.
*/
static bool put_user(uint8_t *udst, uint8_t byte);

/**
 * \brief Checks whether a user string is valid.
 * \return True if the string is valid, false otherwise.
*/
static bool validate_user_string(const char* str);

/**
 * \brief Checks whether the location `buf` pointed to can be read.
*/
static bool validate_read_user_buffer(void *buf, uint32_t size);

/**
 * \brief Check whether the location `dst` pointed to can be written. If successed, 
 * copy `size` bytes from `src` to `dst`.
 * \return True if successful, false otherwise. 
*/
static bool validate_write_user_buffer(void *dst, void *src, uint32_t size);

static void syscall_handler (struct intr_frame *);

// User process syscall handlers
static void syscall_halt_handler(struct intr_frame *);
static void syscall_exit_handler(struct intr_frame *);
static void syscall_exec_handler(struct intr_frame *);
static void syscall_wait_handler(struct intr_frame *);

// File system syscall handlers
static void syscall_create_handler(struct intr_frame *);
static void syscall_remove_handler(struct intr_frame *);
static void syscall_open_handler(struct intr_frame *);
static void syscall_filesize_handler(struct intr_frame *);
static void syscall_read_handler(struct intr_frame *);
static void syscall_write_handler(struct intr_frame *);
static void syscall_seek_handler(struct intr_frame *);
static void syscall_tell_handler(struct intr_frame *);
static void syscall_close_handler(struct intr_frame *);

static void exit_handler(int exit_code);

static bool validate_type(const uint8_t* src, uint8_t* dst, uint8_t size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_id;
  if(!validate_type((const uint8_t*)f->esp, (uint8_t*)&syscall_id, sizeof syscall_id))
    goto Fail;
  switch(syscall_id)
    {
      case SYS_HALT:
        syscall_halt_handler(f);
        break;
      case SYS_EXIT:
        syscall_exit_handler(f);
        break;
      case SYS_EXEC:
        syscall_exec_handler(f);
        break;
      case SYS_WAIT:
        syscall_wait_handler(f);
        break;
      case SYS_CREATE:
        syscall_create_handler(f);
        break;
      case SYS_REMOVE:
        syscall_remove_handler(f);
        break;
      case SYS_OPEN:
        syscall_open_handler(f);
        break;
      case SYS_FILESIZE:
        syscall_filesize_handler(f);
        break;
      case SYS_READ:
        syscall_read_handler(f);
        break;
      case SYS_WRITE:
        syscall_write_handler(f);
        break;
      case SYS_SEEK:
        syscall_seek_handler(f);
        break;
      case SYS_TELL:
        syscall_tell_handler(f);
        break;
      case SYS_CLOSE:
        syscall_close_handler(f);
        break;
      default:
        goto Fail;
    }
    return;
    Fail:
      exit_handler(-1);
}

static int get_user(const uint8_t *uaddr)
{
  if((const void*)uaddr >= PHYS_BASE)
    return -1;
  int result;
  asm volatile (
    "movl $1f, %0; movzbl %1, %0; 1:"
    : "=&a" (result) : "m" (*uaddr)
  );
  return result;
}

static bool put_user(uint8_t *udst, uint8_t byte)
{
  if((void*)udst >= PHYS_BASE)
    return false;
  int error_code;
  asm volatile (
    "movl $1f, %0; movb %b2, %1; 1:"
    : "=&a" (error_code), "=m" (*udst) : "q" (byte)
  );
  return error_code != -1;
}

static bool validate_user_string(const char* str)
{
  int result;
  while((result = get_user((const uint8_t *)(str++))) != 0) {
    if(result == -1)
      return false;
  }
  return true;
}

static bool validate_read_user_buffer(void *buf, uint32_t size) {
  uint8_t *p_bytes = buf;
  for(uint32_t offset = 0 ; offset < size ; ++offset) {
    if(get_user(p_bytes + offset) == -1)
      return false;
  }
  return true;
}

static bool validate_write_user_buffer(void *dst, void *src, uint32_t size) {
  for(uint32_t offset = 0 ; offset < size ; ++offset) {
    if(!put_user(((uint8_t*)dst) + offset, ((uint8_t*)src)[offset]))
      return false;
  }
  return true;
}



static void syscall_halt_handler(struct intr_frame *f)
{
  shutdown_power_off();
  NOT_REACHED();
}

static void syscall_exit_handler(struct intr_frame *f)
{
  int exit_code;
  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&exit_code, sizeof exit_code))
    goto Failed;
  exit_handler(exit_code);
  Failed:
    exit_handler(-1);
}

static void syscall_exec_handler(struct intr_frame *f)
{
  const char *cmd;
  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&cmd, sizeof cmd)) {
    goto Failed;
  }
  if(!validate_user_string(cmd)) {
    goto Failed;
  } else {
    f->eax = (uint32_t)process_execute(cmd);
    return;
  }
  Failed:
    exit_handler(-1);
}

static void syscall_wait_handler(struct intr_frame *f)
{
  tid_t tid;
  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&tid, sizeof tid))
    goto Failed;
  f->eax = process_wait(tid);
  return;
  Failed:
    exit_handler(-1);
}

static void syscall_create_handler(struct intr_frame *f)
{
  const char *file_name = NULL;
  unsigned initial_size = 0;

  if(
    !validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&file_name, sizeof file_name) ||
    !validate_type((const uint8_t*)f->esp + 8, (uint8_t*)&initial_size, sizeof initial_size)
  )
    goto Failed;


  if(!validate_user_string(file_name))
    goto Failed;
  filesys_lock_acquire();
  f->eax = filesys_create(file_name, initial_size);
  filesys_lock_release();
  return;
  Failed:
    exit_handler(-1);
}

static void syscall_remove_handler(struct intr_frame *f)
{
  const char *file_name;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&file_name, sizeof file_name))
    goto Failed;

  if(!validate_user_string(file_name))
    goto Failed;

  filesys_lock_acquire();
  f->eax = filesys_remove(file_name);
  filesys_lock_release();
  return;
  Failed:
    exit_handler(-1);
}

static void syscall_open_handler(struct intr_frame *f)
{
  const char *file_name = NULL;
  struct file *file = NULL;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&file_name, sizeof file_name))
    goto Failed;
  if(!validate_user_string(file_name))
    goto Failed;
  filesys_lock_acquire();
  file = filesys_open(file_name);
  filesys_lock_release();
  if(!file) {
    f->eax = (uint32_t)-1;
  } else {
    f->eax = allocate_fd(file);
    if(f->eax != (uint32_t)-1) {
      ASSERT(f->eax >= 2);
    }
  }

  return;
  Failed:
    exit_handler(-1);
}

static void syscall_filesize_handler(struct intr_frame *f)
{
  int fd;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&fd, sizeof fd))
    goto Failed;

  struct file *file = get_file(fd);

  if(!file) {
    goto Failed;
  } else {
    filesys_lock_acquire();
    f->eax = file_length(file);
    filesys_lock_release();
  }
  return;
  Failed:
    exit_handler(-1);
  
}

static void syscall_read_handler(struct intr_frame *f)
{
  int fd;
  uint8_t *buf;
  uint32_t size;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&fd, sizeof fd) ||
     !validate_type((const uint8_t*)f->esp + 8, (uint8_t*)&buf, sizeof buf) || 
     !validate_type((const uint8_t*)f->esp + 12, (uint8_t*)&size, sizeof size))
     goto Failed;

  if(fd == STDIN_FILENO) {
    //if `fd` == 0, read from keyboard
    for(uint32_t off = 0 ; off < size ; ++off) {
      buf[off] = input_getc();
    }
    f->eax = size;
  } else {
    struct file *file = NULL;

    //validate we can write `size` bytes into the `buf`.
    for(uint32_t off = 0 ; off < size ; ++off) {
      if(!put_user(buf + off, (uint8_t)0)) {
        goto Failed;
      }
    }

    file = get_file(fd);

    if(!file)
      goto Failed;

    //read from actual file
    filesys_lock_acquire();
    f->eax = file_read(file, buf, size);
    filesys_lock_release();
  }

  return;
  Failed:
    exit_handler(-1);
}

static void syscall_write_handler(struct intr_frame *f)
{
  int fd;
  const uint8_t *buf;
  uint32_t size;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&fd, sizeof fd) ||
     !validate_type((const uint8_t*)f->esp + 8, (uint8_t*)&buf, sizeof buf) ||
     !validate_type((const uint8_t*)f->esp + 12, (uint8_t*)&size, sizeof size))
    goto Failed;

  struct file *file = NULL;

  //validate we can read `size` bytes from the `buf`.
  if(!validate_read_user_buffer((void*)buf, size))
    goto Failed;

  if(fd == STDOUT_FILENO) {
    //if `fd` == 1, write to console
    putbuf((const char*)buf, size);
    f->eax = size;
  } else {
    PANIC("File growth is not supported!");
  }
  return;

  Failed:
    exit_handler(-1);
}

static void syscall_close_handler(struct intr_frame *f)
{
  int fd;

  if(!validate_type((const uint8_t*)f->esp + 4, (uint8_t*)&fd, sizeof fd))
    goto Failed;

  struct file *file = get_file(fd);

  if(!file)
    goto Failed;

  destory_fd(fd);
  return;
  Failed:
    exit_handler(-1);
}

static void syscall_seek_handler(struct intr_frame *f) {
  PANIC("syscall_seek_handler: not implemented");
}

static void syscall_tell_handler(struct intr_frame *f) {
  PANIC("syscall_tell_handler: not implemented");
}

static void exit_handler(int exit_code)
{
  struct thread *cur = thread_current();
  struct thread *par = cur->parent;
  struct process_ret_frame *ret_frame = NULL;
  lock_acquire(&par->lock);
  for(struct list_elem *iter = list_begin(&par->children_list) ; 
      iter != list_end(&par->children_list) ; iter = list_next(iter))
    {
      ret_frame = list_entry(iter, struct process_ret_frame, elem);
      if(ret_frame->tid == cur->tid)
        {
          ret_frame->exit_code = exit_code;
          goto success;
        }
    }
  PANIC("syscall_exit_handler: failed to find children");
  success:
  lock_release(&par->lock);
  thread_exit();
}

static bool validate_type(const uint8_t* src, uint8_t* dst, uint8_t size) {
  static const int mask = 0x000000FF;
  int tmp;
  for(int off = 0 ; off < size ; ++off) {
    tmp = get_user(src + off);
    if(tmp != -1) {
      dst[off] = (uint8_t)(mask & tmp);
    } else {
      return false;
    }
  }
  return true;
}
