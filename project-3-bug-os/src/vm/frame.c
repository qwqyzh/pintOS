#include "vm/frame.h"
#include "list.h"
#include "stdbool.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "filesys/file.h"

extern struct lock file_lock;

static struct list frame_table;
static struct lock frame_table_lock;

void
frame_table_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_table_lock);
}

struct frame_table_entry*
frame_table_entry_insert (uint32_t *frame_addr, struct sup_page_table_entry *aux)
{
  struct frame_table_entry *fte = 
    (struct frame_table_entry *) malloc (sizeof (struct frame_table_entry));
  if (fte == NULL) {
    return NULL;
  }
  fte->frame_addr = frame_addr;
  fte->owner = thread_current ();
  fte->aux = aux;
  lock_acquire (&frame_table_lock);
  list_push_back (&frame_table, &fte->elem);
  lock_release (&frame_table_lock);
  return fte;
}

static void
frame_table_entry_free (struct frame_table_entry *fte)
{
  list_remove (&fte->elem);
  palloc_free_page (fte->frame_addr);
  free (fte);
}

void
frame_free_page (void *frame_addr)
{
  if (frame_addr == NULL)
    return ;
  lock_acquire (&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
    struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
    if (fte->frame_addr == frame_addr) {
      frame_table_entry_free (fte);
      break;
    }
  }
  lock_release (&frame_table_lock);
}

void
remove_all_frames (tid_t pid)
{
  lock_acquire (&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
    struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
    if (fte->owner->tid == pid) {
      list_remove (&fte->elem);
      free (fte);
    }
  }
  lock_release (&frame_table_lock);
}

struct frame_table_entry*
frame_new_page (struct sup_page_table_entry *aux)
{
  if (aux == NULL)
    return NULL;  
  uint32_t *frame_addr = palloc_get_page (PAL_USER);
  if (frame_addr == NULL) {
    lock_acquire (&aux->page_lock);
    struct frame_table_entry *fte = frame_table_evict ();
    if (fte == NULL) {
      lock_release (&aux->page_lock);
      return NULL;
    }
    fte->aux = aux;
    fte->owner = thread_current ();
    lock_release (&aux->page_lock);
    return fte;
  }
  struct frame_table_entry *fte = frame_table_entry_insert (frame_addr, aux);
  if (fte == NULL) {
    palloc_free_page (frame_addr);
    return NULL;
  }
  return fte;
}

static bool
LRU_cmp (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *fte_a = list_entry (a, struct frame_table_entry, elem);
  struct frame_table_entry *fte_b = list_entry (b, struct frame_table_entry, elem);
  bool result = fte_a->aux->access_time < fte_b->aux->access_time;
  // maybe bug
  if (fte_a->aux->writable != fte_b->aux->writable) {
    return fte_a->aux->writable;
  }
  if (is_kernel_vaddr (fte_a->aux) != is_kernel_vaddr (fte_b->aux)) {
    return !is_kernel_vaddr (fte_a->aux);
  }
  return result;
}

struct frame_table_entry*
frame_table_evict (void)
{
  /* Least Recently Used */
  struct list_elem *e = list_min (&frame_table, LRU_cmp, NULL);
  struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
  lock_acquire (&frame_table_lock);
  // from file and is mmap and is dirty
  if (fte->aux->from_file && fte->aux->is_mmap
      && pagedir_is_dirty (thread_current ()->pagedir, fte->aux->user_vaddr)) {
    lock_acquire (&file_lock);
    file_seek (fte->aux->file, fte->aux->offset);
    file_write (fte->aux->file, fte->aux->user_vaddr,
                fte->aux->read_bytes);
    lock_release (&file_lock);
  }
  else {
    fte->aux->from_file = false;
    write_from_block (fte);
  }
  pagedir_clear_page (fte->owner->pagedir,
                      fte->aux->user_vaddr);
  lock_release (&frame_table_lock);
  return fte;
}