#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>
#include <stdio.h>
#include "user/syscall.h"
#include "threads/thread.h"

struct mmap_entry {
  int mapid;
  int pc; // page count
  void *addr;
  struct file *file;
  struct list_elem elem;
};

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
void syscall_munmap (int mapid);
int syscall_mmap (int fd, void *addr);

#endif /* userprog/syscall.h */
