#include "vm/page.h"
#include <hash.h>
#include "stdbool.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include <string.h>
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

extern struct lock file_lock;
extern bool install_page (void*, void*, bool);

static unsigned
page_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
  const struct sup_page_table_entry *spte
       = hash_entry (elem, struct sup_page_table_entry, elem);
  return hash_bytes (&spte->user_vaddr, sizeof (spte->user_vaddr));
}

static bool
page_addr_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
  const struct sup_page_table_entry *spte_a
      = hash_entry (a, struct sup_page_table_entry, elem);
  const struct sup_page_table_entry *spte_b
      = hash_entry (b, struct sup_page_table_entry, elem);
  return spte_a->user_vaddr < spte_b->user_vaddr;
}

void
sup_page_table_init (struct hash *sup_page_table)
{
  hash_init (sup_page_table, page_hash_func, page_addr_less_func, NULL);
}

struct sup_page_table_entry *
new_sup_table_entry (void *addr, uint64_t access_time)
{
  struct sup_page_table_entry*
    spte = (struct sup_page_table_entry *)malloc (sizeof (struct sup_page_table_entry));
  if (spte == NULL)
    return NULL;
  spte->user_vaddr = pg_round_down (addr);
  spte->access_time = access_time;
  spte->sector_idx = NO_SWAP_INDEX;
  spte->from_file = false;
  spte->file = NULL;
  spte->offset = 0;
  spte->read_bytes = 0;
  spte->zero_bytes = 0;
  spte->writable = false;
  spte->is_mmap = false;
  lock_init (&spte->page_lock);
  return spte;
}

static void
sup_page_table_entry_dtor (struct hash_elem *e, void *aux UNUSED)
{
  struct sup_page_table_entry*
      spte = hash_entry (e, struct sup_page_table_entry, elem);
  if (spte->sector_idx != NO_SWAP_INDEX)
    release_swap_slot (spte->sector_idx);
  free (spte);
}

void
sup_page_table_free (struct hash *sup_page_table)
{
  hash_destroy (sup_page_table, sup_page_table_entry_dtor);
}

struct sup_page_table_entry*
sup_table_find (struct hash *table, void *page)
{
  if (table == NULL || page == NULL)
    return NULL;
  struct sup_page_table_entry spte;
  spte.user_vaddr = pg_round_down (page);
  struct hash_elem *e = hash_find (table, &spte.elem);
  if (e == NULL)
    return NULL;
  return hash_entry (e, struct sup_page_table_entry, elem);
}

bool
try_get_page (void *fault_addr, void *esp)
{
  struct sup_page_table_entry*
      spte = sup_table_find (&thread_current ()->sup_page_table, fault_addr);
  if (spte == NULL) {
      if ((uint32_t)fault_addr < (uint32_t)esp - 32)
        return false;
      return grow_stack (fault_addr);
    }
  else if (spte->from_file) 
    return load_from_file (spte);
  else 
    return load_from_swap (spte);
}

bool
grow_stack (void *fault_addr)
{
  struct thread *cur = thread_current ();
  struct sup_page_table_entry*
      spte = new_sup_table_entry (fault_addr, timer_ticks ());
  if (spte == NULL)
    return false;
  struct frame_table_entry*
      fte = frame_new_page (spte);
  if (fte == NULL) {
    free (spte);
    return false;
  }
  void *frame = fte->frame_addr;
  if (install_page (spte->user_vaddr, frame, true) && 
    !hash_insert (&cur->sup_page_table, &spte->elem)) {
      return true;
  }
  else {
      free (spte);
      frame_free_page (frame);
      return false;
  }
}

bool
load_from_swap (struct sup_page_table_entry *spte)
{
  struct frame_table_entry*
    fte = frame_new_page (spte);
  lock_acquire (&spte->page_lock);
  read_from_block (fte, spte->sector_idx);
  spte->sector_idx = NO_SWAP_INDEX;
  spte->access_time = timer_ticks ();
  if (install_page (spte->user_vaddr, fte->frame_addr, spte->writable)) {
    lock_release (&spte->page_lock);
    return true;
  }
  else {
    frame_free_page (fte->frame_addr);
    hash_delete (&thread_current ()->sup_page_table, &spte->elem);
    lock_release (&spte->page_lock);
    return false;
  }
}

bool
load_from_file (struct sup_page_table_entry *spte)
{
  struct frame_table_entry*
    fte = frame_new_page (spte);
  if (spte == NULL)
    return false;
  lock_acquire (&spte->page_lock);
  void *frame = fte->frame_addr;
  lock_acquire (&file_lock);
  file_seek (spte->file, spte->offset);
  if (file_read (spte->file, frame, spte->read_bytes) != (int)spte->read_bytes) {
    frame_free_page (frame);
    lock_release (&file_lock);
    lock_release (&spte->page_lock);
    return false;
  }
  lock_release (&file_lock);
  memset (frame + spte->read_bytes, 0, spte->zero_bytes);
  if (!install_page (spte->user_vaddr, frame, spte->writable)) {
    frame_free_page (frame);
    lock_release (&spte->page_lock);
    return false;
  }
  lock_release (&spte->page_lock);
  return true;
}

bool
lazy_load (struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes,
           uint32_t zero_bytes, bool writable, bool is_mmap)
{
  int32_t offset = ofs;
  while (read_bytes > 0 || zero_bytes > 0) {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    struct sup_page_table_entry*
        spte = new_sup_table_entry (upage, timer_ticks ());
    if (spte == NULL)
      return false;
    spte->from_file = true;
    spte->file = file;
    spte->read_bytes = page_read_bytes;
    spte->zero_bytes = page_zero_bytes;
    spte->writable = writable;
    spte->offset = offset;
    spte->is_mmap = is_mmap;

    if (hash_insert (&thread_current()->sup_page_table, &spte->elem)) {
      free (spte);
      return false;
    }

    offset += page_read_bytes;
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}