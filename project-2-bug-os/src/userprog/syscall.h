#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>
#include <stdio.h>
#include "user/syscall.h"
/* vm */
typedef int mapid_t;
typedef struct mmap_entry
{
  mapid_t id;            /* Memory map id */
  void *addr;            /* Virtual address of mmap */
  struct file *file;     /* Mapped file */
  int page_count;        /* Number of page mapped from file */
  struct list_elem elem; /* Elem for list sturct */
} mmap_entry_t;

void syscall_init (void);

void syscall_exit (int status);
void syscall_halt (void);
pid_t syscall_exec (const char *cmd_line);
int syscall_wait (pid_t pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void* buffer, unsigned size);
int syscall_write (int fd, const void* buffer, unsigned size);
int syscall_seek (int fd, unsigned position);
int syscall_tell (int fd);
void syscall_close (int fd);

/* vm */
mapid_t syscall_mmap (int fd, void *addr);
void syscall_munmap (mapid_t mapping);
#endif /* userprog/syscall.h */
