#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

#define NO_SWAP_INDEX (-1)

void swap_init (void);
void swap_free (void);
int find_swap_slot (void);
void release_swap_slot (int index);
void read_from_block (struct frame_table_entry *frame, int index);
void write_from_block (struct frame_table_entry *frame);

#endif