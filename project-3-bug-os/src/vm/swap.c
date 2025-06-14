#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include <bitmap.h>
#include <stddef.h>

static struct block *global_swap_block;
static struct bitmap *swap_block_bitmap;
static struct lock swap_lock;

void
swap_init (void)
{
  global_swap_block = block_get_role (BLOCK_SWAP);
  swap_block_bitmap = bitmap_create (block_size (global_swap_block));
  if (swap_block_bitmap == NULL) {
    return ;
  }
  lock_init (&swap_lock);
}

void
swap_free (void)
{
  bitmap_destroy (swap_block_bitmap);
}

int
find_swap_slot (void)
{
  lock_acquire (&swap_lock);
  size_t index = bitmap_scan_and_flip (swap_block_bitmap, 0, 8, false);
  if (index == BITMAP_ERROR) {
    lock_release (&swap_lock);
    syscall_exit(-1);
  }
  lock_release (&swap_lock);
  return index;
}

void
release_swap_slot (int index)
{
  lock_acquire (&swap_lock);
  bitmap_set_multiple (swap_block_bitmap, index, 8, false);
  lock_release (&swap_lock);
}

void
read_from_block (struct frame_table_entry *frame, int index)
{
  /*the frame is the frame I want to read into
  the index is the starting index of the block that is free*/
  for (int i = 0; i < 8; ++i) {
    /* each read will read 512 bytes, therefore we need to read 8 times,
     each at 512 increments of the frame*/
    block_read (global_swap_block, index + i, frame->frame_addr + (i * BLOCK_SECTOR_SIZE));
  }
  /* Don’t need the data in the block anymore, so set the index of the
   block you just read from as free. */
  release_swap_slot (index);
}

void
write_from_block (struct frame_table_entry *frame)
{
  int index = find_swap_slot ();
  frame->aux->sector_idx = index;
  for (int i = 0; i < 8; ++i) {
    block_write (global_swap_block, index + i, frame->frame_addr + (i * BLOCK_SECTOR_SIZE));
  }
}