#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "list.h"
#include "stdbool.h"
#include "stddef.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "user/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "vm/page.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);

static void object_validation (const void *ptr, size_t size);
static void pointer_validation (const void *ptr, uint32_t *pd);
static void string_validation (const void *ptr);
static struct file_list_elem *get_file_list_elem (int fd);
static struct mmap_entry *new_mmap_entry (void *addr, struct file *file, int pc);
static bool check_mmap_overlaps (void *addr, int size);

// one open file, one file_list_elem
struct file_list_elem {
  struct file *file;
  int fd; // file descriptor
  struct list_elem elem;
};

struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  object_validation (f->esp, sizeof(void*));
  switch (*(int*)f->esp) {
    case SYS_HALT: {
      syscall_halt ();
      break;
    }
    case SYS_EXIT: {
      object_validation (f->esp + 1, sizeof(void*));
      int status = *((int*)f->esp + 1);
      syscall_exit (status);
      break;
    }
    case SYS_EXEC: {
      object_validation (f->esp + 1, sizeof(void*));
      const char *cmd_line = (const char*)(*((int*)f->esp + 1));
      f->eax = syscall_exec (cmd_line);
      break;
    }
    case SYS_WAIT: {
      object_validation (f->esp + 1, sizeof(void*));
      int pid = *((int*)f->esp + 1);
      f->eax = syscall_wait (pid);
      break;
    }
    case SYS_CREATE: {
      object_validation (f->esp + 1, 2 * sizeof(void*));
      const char *file = (const char*)(*((int*)f->esp + 1));
      unsigned initial_size = *((unsigned*)f->esp + 2);
      f->eax = syscall_create (file, initial_size);
      break;
    }
    case SYS_REMOVE: {
      object_validation (f->esp + 1, sizeof(void*));
      const char *file = (const char*)(*((int*)f->esp + 1));
      f->eax = syscall_remove (file);
      break;
    }
    case SYS_OPEN: {
      object_validation (f->esp + 1, sizeof(void*));
      const char *file = (const char*)(*((int*)f->esp + 1));
      f->eax = syscall_open (file);
      break;
    }
    case SYS_FILESIZE: {
      object_validation (f->esp + 1, sizeof(void*));
      int fd = *((int*)f->esp + 1);
      f->eax = syscall_filesize (fd);
      break;
    }
    case SYS_READ: {
      object_validation (f->esp + 1, 3 * sizeof(void*));
      int fd = *((int*)f->esp + 1);
      void* buffer = (void*)(*((int*)f->esp + 2));
      unsigned size = *((unsigned*)f->esp + 3);
      f->eax = syscall_read (fd, buffer, size);
      break;
    }
    case SYS_WRITE: {
      object_validation (f->esp + 1, 3 * sizeof(void*));
      int fd = *((int*)f->esp + 1);
      const void* buffer = (const void*)(*((int*)f->esp + 2));
      unsigned size = *((unsigned*)f->esp + 3);
      f->eax = syscall_write (fd, buffer, size);
      break;
    }
    case SYS_SEEK: {
      object_validation (f->esp + 1, 2 * sizeof(void*));
      int fd = *((int*)f->esp + 1);
      unsigned position = *((unsigned*)f->esp + 2);
      syscall_seek (fd, position);
      break;
    }
    case SYS_TELL: {
      object_validation (f->esp + 1, sizeof(void*));
      int fd = *((int*)f->esp + 1);
      f->eax = syscall_tell (fd);
      break;
    }
    case SYS_CLOSE: {
      object_validation (f->esp + 1, sizeof(void*));
      int fd = *((int*)f->esp + 1);
      syscall_close (fd);
      break;
    }
    case SYS_MMAP: {
      object_validation (f->esp + 1, 2 * sizeof(void*));
      int fd = *((int*)f->esp + 1);
      void *addr = (void*)(*((int*)f->esp + 2));
      f->eax = syscall_mmap (fd, addr);
      break;
    }
    case SYS_MUNMAP: {
      object_validation (f->esp + 1, sizeof(void*));
      int mapid = *((int*)f->esp + 1);
      syscall_munmap (mapid);
      break;
    }
    default: {
      syscall_exit (-1);
      break;
    }
  }
}

void
syscall_exit (int status)
{
  struct thread *t = thread_current ();
  while (!list_empty (&t->file_list)) {
    struct list_elem *e = list_pop_back (&t->file_list);
    struct file_list_elem *fle = list_entry (e, struct file_list_elem, elem);
    file_close (fle->file);
    free (fle);
  }
  t->cth->exit_status = status;
  thread_exit ();
}

void
syscall_halt (void)
{
  shutdown_power_off ();
}

pid_t
syscall_exec (const char *cmd_line)
{
  string_validation (cmd_line);
  lock_acquire (&file_lock);
  pid_t pid = process_execute (cmd_line);
  lock_release (&file_lock);
  if (pid == TID_ERROR) {
    return -1;
  }
  struct child_thread *child = get_child_thread (pid);
  if (!child) {
    return -1;
  }
  sema_down (&child->sema_exec);
  if (child->status == CHILD_THREAD_KILLED) {
    sema_down (&child->sema_wait);
    list_remove (&child->elem);
    free (child);
    return -1;
  }
  return pid;
}

int
syscall_wait (pid_t pid)
{
  return process_wait (pid);
}

bool
syscall_create (const char *file, unsigned initial_size)
{
  string_validation (file);
  lock_acquire (&file_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_lock);
  return success;
}

bool
syscall_remove (const char *file)
{
  string_validation (file);
  lock_acquire (&file_lock);
  bool success = filesys_remove (file);
  lock_release (&file_lock);
  return success;
}

int
syscall_open (const char *file)
{
  string_validation (file);
  lock_acquire (&file_lock);
  struct file *f = filesys_open (file);
  lock_release (&file_lock);
  if (f == NULL) {
    return -1;
  }
  struct thread *cur = thread_current ();
  struct file_list_elem *open_file = malloc (sizeof(struct file_list_elem));
  if (open_file == NULL) {
    return -1;
  }
  open_file->fd = cur->tfd++;
  open_file->file = f;
  list_push_back (&cur->file_list, &open_file->elem);
  return open_file->fd;
}

int
syscall_filesize (int fd)
{
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  int filesize = file_length (fle->file);
  lock_release (&file_lock);
  return filesize;
}

int 
syscall_read (int fd, void *buffer, unsigned size)
{
  object_validation (buffer, size);
  string_validation (buffer);
  if (fd == STDIN_FILENO) {
    for (unsigned i = 0; i < size; i++) {
      ((char*)buffer)[i] = input_getc ();
    }
    return size;
  }
  if (fd == STDOUT_FILENO) {
    syscall_exit (-1);
  }
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  int len = file_read (fle->file, buffer, size);
  lock_release (&file_lock);
  return len;
}

int
syscall_write (int fd, const void *buffer, unsigned size)
{
  object_validation (buffer, size);
  if (fd == STDOUT_FILENO) {
    putbuf ((const char*)buffer, size);
    return size;
  }
  if (fd == STDIN_FILENO) {
    syscall_exit (-1);
  }
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  int len = file_write (fle->file, buffer, size);
  lock_release (&file_lock);
  return len;
}

int
syscall_seek (int fd, unsigned position)
{
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  file_seek (fle->file, position);
  lock_release (&file_lock);
  return 0;
}

int
syscall_tell (int fd)
{
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  int pos = file_tell (fle->file);
  lock_release (&file_lock);
  return pos;
}

void
syscall_close (int fd)
{
  struct file_list_elem *fle = get_file_list_elem (fd);
  lock_acquire (&file_lock);
  file_close (fle->file);
  lock_release (&file_lock);
  list_remove (&fle->elem);
  free (fle);
}

int
syscall_mmap (int fd, void *addr)
{
  if (fd < 2 || (uint32_t)addr % PGSIZE)
    return -1;
  struct file_list_elem *fle = get_file_list_elem (fd);
  off_t file_size = 0;
  if (fle->file == NULL || !(file_size = file_length (fle->file)))
    return -1;
  lock_acquire (&file_lock);
  struct file *f = file_reopen (fle->file);
  lock_release (&file_lock);
  if (!f || !check_mmap_overlaps (addr, file_size))
    return -1;
  uint32_t read_bytes = file_size;
  uint32_t zero_bytes = (PGSIZE - read_bytes % PGSIZE) % PGSIZE;
  int page_count = (read_bytes + zero_bytes) / PGSIZE;
  struct mmap_entry *me = new_mmap_entry (addr, f, page_count);
  if (!lazy_load (f, 0, addr, read_bytes, zero_bytes, true, true)) {
    free (me);
    return -1;
  }
  list_push_back (&thread_current ()->mmap_list, &me->elem);
  return me->mapid;
}

void
syscall_munmap (int mapid)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&cur->mmap_list); e != list_end (&cur->mmap_list);
       e = list_next (e)) {
    struct mmap_entry *me = list_entry (e, struct mmap_entry, elem);
    if (me->mapid == mapid) {
      list_remove (&me->elem);
      // Free mmap entry
      void *addr = me->addr;
      // Iterate through all mapped page and release resources
      for (int cur_page = 0; cur_page < me->pc; ++cur_page)  {
        struct sup_page_table_entry *spte = sup_table_find (&cur->sup_page_table, addr);
        if (spte) {
            if (pagedir_is_dirty (cur->pagedir, addr)) {
              lock_acquire (&file_lock);
              file_seek (spte->file, spte->offset);
              file_write (spte->file, addr, spte->read_bytes);
              lock_release (&file_lock);
            }
            if (pagedir_get_page (cur->pagedir, spte->user_vaddr)) {
              frame_free_page (
                  pagedir_get_page (cur->pagedir, spte->user_vaddr));
              pagedir_clear_page (cur->pagedir, spte->user_vaddr);
            }
            hash_delete (&cur->sup_page_table, &spte->elem);
          }
        addr += PGSIZE;
      }
      lock_acquire (&file_lock);
      file_close (me->file);
      lock_release (&file_lock);
      free (me);
      break;
    }
  }
}

static struct mmap_entry*
new_mmap_entry (void *addr, struct file *file, int pc)
{
  struct mmap_entry *
      me = (struct mmap_entry *)malloc (sizeof (struct mmap_entry));
  if (me == NULL)
    return NULL;
  me->mapid = thread_current ()->mmap_id++;
  me->addr = addr;
  me->file = file;
  me->pc = pc;
  return me;
}

static bool
check_mmap_overlaps (void *addr, int size)
{
  if (addr == NULL || size < 0)
    return false;
  struct thread *cur = thread_current ();
  while (size >= 0) {
    if (sup_table_find (&cur->sup_page_table, addr) || 
        pagedir_get_page (cur->pagedir, addr)) {
      return false;
    }
    addr += PGSIZE;
    size -= PGSIZE;
  }
  return true;
}

static void
object_validation (const void *ptr, size_t size)
{
  uint32_t *pd = thread_current ()->pagedir;
  pointer_validation (ptr, pd);
  pointer_validation (ptr + size - 1, pd);
}

static void
pointer_validation (const void *ptr, uint32_t *pd)
{
  if (ptr == NULL || !is_user_vaddr (ptr) || ptr < (void*)0x08048000
   || pagedir_get_page (pd, ptr) == NULL) {
    syscall_exit (-1);
  }
}

static void
string_validation (const void *ptr)
{
  uint32_t *pd = thread_current ()->pagedir;
  while (true) {
    pointer_validation (ptr, pd);
    if (*(char*)ptr == '\0') {
      break;
    }
    ptr++;
  }
}

static struct file_list_elem *
get_file_list_elem (int fd)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&cur->file_list); e != list_end (&cur->file_list);
       e = list_next (e)) {
    struct file_list_elem *fle = list_entry (e, struct file_list_elem, elem);
    if (fle->fd == fd) {
      return fle;
    }
  }
  syscall_exit (-1);
  return NULL;
}
