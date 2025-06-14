#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H
#include <stdbool.h>
#include "devices/block.h"

#define BUFFER_CACHE_SIZE 64

struct buffer_cache
{
  bool is_inuse;                      /* is entry in use */
  bool is_dirty;                      /* is cache dirty */
  int64_t lst_time;                   /* Last access time */
  block_sector_t sector;              /* sector */
  uint8_t block_data[BLOCK_SECTOR_SIZE];    /* Block data */
};

void buffer_cache_init (void);

void buffer_cache_close (void);

void buffer_cache_read (block_sector_t sector, void *buffer);

void buffer_cache_write (block_sector_t sector, const void *buffer);

#endif