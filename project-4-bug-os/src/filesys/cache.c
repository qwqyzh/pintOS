#include "filesys/cache.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include <string.h>

static struct buffer_cache caches[BUFFER_CACHE_SIZE];
static struct lock buffer_cache_lock;
extern struct block *fs_device;

static void
buffer_cache_entry_init (struct buffer_cache *entry, block_sector_t sector)
{
  entry->is_inuse = false;
  entry->is_dirty = false;
  entry->sector = sector;
  entry->lst_time = timer_ticks ();
  block_read (fs_device, sector, entry->block_data);
}

void
buffer_cache_init ()
{
  lock_init (&buffer_cache_lock);
  memset (caches, 0, sizeof (caches));
}

static void
buffer_cache_entry_flush (struct buffer_cache *entry)
{
  if (entry->is_inuse && entry->is_dirty)
    block_write (fs_device, entry->sector, entry->block_data);
}

static int /* return index */
evict_buffer_cache (void)
{
  int64_t min_time = caches[0].lst_time;
  int recent_index = 0;
  for (int i = 1; i < BUFFER_CACHE_SIZE; ++i)
    {
      if (caches[i].lst_time < min_time) /*find the recent used cache*/
        {
          min_time = caches[i].lst_time;
          recent_index = i;
        }
    }
  buffer_cache_entry_flush (&caches[recent_index]);
  return recent_index;
}

static int /* return index */
get_buffer_cache (block_sector_t sector)
{
  int now_sector_index = -1, free_index = -1;
  for (int i = 0; i < BUFFER_CACHE_SIZE; ++i)
    {
      if (free_index == -1 && !caches[i].is_inuse)free_index = i;
      if (caches[i].sector == sector)
        {
          now_sector_index = i;
          break;
        }
    }
  if (now_sector_index != -1) //just use it
    {
      caches[now_sector_index].lst_time = timer_ticks ();
      return now_sector_index;
    }
  if (free_index == -1)
    free_index = evict_buffer_cache ();
  buffer_cache_entry_init (&caches[free_index], sector);
  return free_index;
}

void
buffer_cache_close ()
{
  lock_acquire (&buffer_cache_lock);
  for (int i = 0; i < BUFFER_CACHE_SIZE; ++i)
    buffer_cache_entry_flush (caches + i);
  lock_release (&buffer_cache_lock);
}

void
buffer_cache_read (block_sector_t sector, void *buffer)
{
  lock_acquire (&buffer_cache_lock);
  int index = get_buffer_cache (sector);
  memcpy (buffer, caches[index].block_data, BLOCK_SECTOR_SIZE);
  lock_release (&buffer_cache_lock);
}

void
buffer_cache_write (block_sector_t sector, const void *buffer)
{
  lock_acquire (&buffer_cache_lock);
  int index = get_buffer_cache (sector);
  caches[index].is_dirty = true;
  memcpy (caches[index].block_data, buffer, BLOCK_SECTOR_SIZE);
  lock_release (&buffer_cache_lock);
}