#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "hash.h"
#include "threads/synch.h"

struct sup_page_table_entry {
  uint32_t *user_vaddr;
  uint64_t access_time;

  struct lock page_lock;
  struct file *file;
  struct hash_elem elem;

  int sector_idx;
  int32_t offset;
  int32_t read_bytes;
  int32_t zero_bytes;

  bool writable;
  bool from_file;
  bool is_mmap;
};

void sup_page_table_init (struct hash *sup_page_table);
struct sup_page_table_entry *new_sup_table_entry (void *addr, uint64_t access_time);
bool sup_page_table_insert (struct hash *sup_page_table, struct sup_page_table_entry *spte);
struct sup_page_table_entry *sup_page_table_find (struct hash *sup_page_table, void *addr);
bool sup_page_table_delete (struct hash *sup_page_table, struct sup_page_table_entry *spte);
void sup_page_table_free (struct hash *sup_page_table);
struct sup_page_table_entry *sup_table_find (struct hash *table, void *page);
bool try_get_page (void *fault_addr, void *esp);
bool grow_stack (void *fault_addr);
bool load_from_swap (struct sup_page_table_entry *spte);
bool load_from_file (struct sup_page_table_entry *spte);
bool lazy_load (struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes,
           uint32_t zero_bytes, bool writable, bool is_mmap);

#endif