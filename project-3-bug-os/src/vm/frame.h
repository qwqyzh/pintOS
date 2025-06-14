#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <list.h>
#include "vm/page.h"
#include "threads/thread.h"

struct frame_table_entry {
  struct list_elem elem;
  uint32_t *frame_addr;
  struct thread *owner;
  struct sup_page_table_entry *aux;
};

void frame_table_init (void);
struct frame_table_entry* frame_table_entry_insert (uint32_t *frame_addr, struct sup_page_table_entry *aux);
struct frame_table_entry* frame_new_page (struct sup_page_table_entry *aux);
struct frame_table_entry* frame_table_evict (void);
void frame_free_page (void *frame_addr);
void remove_all_frames (tid_t pid);


#endif