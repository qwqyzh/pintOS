             +-------------------------+
             |         CS 140          |
             | PROJECT 4: FILE SYSTEMS |
             |     DESIGN DOCUMENT     |
             +-------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Zhechuan Yu <arahato@shanghaitech.edu.cn>
Ziheng Yang <yangzh2023@shanghaitech.edu.cn>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

             INDEXED AND EXTENSIBLE FILES
             ============================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

```c
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };

struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
```
>> A2: What is the maximum size of a file supported by your inode
>> structure?  Show your work.

direct data block: 123

indirect data block: 1

double indirect block: 1

total blocks: 123 + 1 * 128 + 1 * 128 * 128 = 16635

maximum size: 16635 * 512B = 8.12255859375 MB

---- SYNCHRONIZATION ----

>> A3: Explain how your code avoids a race if two processes attempt to extend a file at the same time.

If a thread wants to extend a block, it does the following: 

First, Acquire an exclusive lock on the inode that points to the block we want to extend.
Exclusive lock. 

Then, it allocates the block in disk using "free_map_allocate", acquires the exclusive lock on that block, and clears theblock in disk, acquires the exclusive lock on that block, and clears it. 

Then, update the value of the sector number in the inode and releases the inode's exclusive lock.It then updates the value of the sector number in the inode and releases the inode's exclusive lock. 

Finally, it writes to the new block and releases the exclusive lock on that block. The exclusive lock on the inode ensures that the expansion does not occur twice.

It does not happen twice. An exclusive lock on a sector ensures that writes to new sectors are atomic.Thus, races are prevented.

>> A4: Suppose processes A and B both have file F open, both
>> positioned at end-of-file.  If A reads and B writes F at the same
>> time, A may read all, part, or none of what B writes.  However, A
>> may not read data other than what B writes, e.g. if B writes
>> nonzero data, A is not allowed to see all zeros.  Explain how your code avoids this race.

A's function is constrained by the current length of file, and after B has finished the whole extension of file, the length will be updated.

As a result, A will read after B writes and updates the length.
>> A5: Explain how your synchronization design provides "fairness".
>> File access is "fair" if readers cannot indefinitely block writers
>> or vice versa.  That is, many processes reading from a file cannot
>> prevent forever another process from writing the file, and many
>> processes writing to a file cannot prevent another process forever
>> from reading the file.

We use shared lock (read/write lock).And each cache has a shared lock with it. 
Multiple threads can access a file without acquiring a lock. 
But if they want to access the same block of the file, there’ll be synchronization that they must acquire shared_lock of this block first.

“Fairness” problem will happen when many threads want to
read/write one particular block.And our design of shared_lock will prevent starvation


---- RATIONALE ----

>> A6: Is your inode structure a multilevel index?  If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks?  If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have, compared to a multilevel index?

Yes, and it has double indirect block.1 double indirect block is sufficient for 8MB capacity and 1 indirect block for about 64kb size file. The other 123 pointers are used for direct data block. 

                SUBDIRECTORIES
                ==============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
```c
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };
```

---- ALGORITHMS ----

>> B2: Describe your code for traversing a user-specified path.  How
>> do traversals of absolute and relative paths differ?

---- SYNCHRONIZATION ----

>> B4: How do you prevent races on directory entries?  For example,
>> only one of two simultaneous attempts to remove a single file
>> should succeed, as should only one of two simultaneous attempts to
>> create a file with the same name, and so on.

>> B5: Does your implementation allow a directory to be removed if it
>> is open by a process or if it is in use as a process's current
>> working directory?  If so, what happens to that process's future
>> file system operations?  If not, how do you prevent it?

---- RATIONALE ----

>> B6: Explain why you chose to represent the current directory of a
>> process the way you did.

                 BUFFER CACHE
                 ============

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

```c
struct buffer_cache
{
  bool is_inuse;                      /* is entry in use */
  bool is_dirty;                      /* is cache dirty */
  int64_t lst_time;                   /* Last access time */
  block_sector_t sector;              /* sector */
  uint8_t block_data[BLOCK_SECTOR_SIZE];    /* Block data */
};
```
---- ALGORITHMS ----

>> C2: Describe how your cache replacement algorithm chooses a cache
>> block to evict.

find the least recently used block by comparing the last access time to evict

>> C3: Describe your implementation of write-behind.

We keep dirty blocks in the cache, and write them to disk whenever they are evicted

>> C4: Describe your implementation of read-ahead.

When we read a block in function “inode_read”, we find a free cache block  with "get_buffer_cache" and do the reading function.

---- SYNCHRONIZATION ----

>> C5: When one process is actively reading or writing data in a
>> buffer cache block, how are other processes prevented from evicting
>> that block?

By using non-exclusive lock and exclusive lock.
If a process P wants to read cache block A, it must hold the 
non-exclusive lock of A. If P is wants to write cache block A, it must
hold the exclusive lock of A. When another process Q wants to evict A, 
it must acquire the locks first. Q will fail to get the lock, thus the evicting will fail.

>> C6: During the eviction of a block from the cache, how are other
>> processes prevented from attempting to access the block?

By using non-exclusive lock and exclusive lock.
Process P will hold the the exclusive lock of A when evicting it.
Thus other processes can't acquire any locks, so they're prevented from the access to A.

---- RATIONALE ----

>> C7: Describe a file workload likely to benefit from buffer caching,
>> and workloads likely to benefit from read-ahead and write-behind.

Database Servers: Workloads involving frequent database queries benefit greatly from buffer caching. The database management system (DBMS) caches frequently accessed data pages in memory, reducing the need to fetch them from disk repeatedly.

Batch Processing: Workloads involving large-scale data processing, such as data analytics jobs or batch file processing, benefit from read-ahead. Pre-fetching data into the cache allows subsequent processing steps to operate more smoothly without waiting for disk I/O.

Transactional Systems: Systems that handle frequent small writes, such as transactional databases, benefit from write-behind caching. Write-behind delays the actual writing of data to disk, batching multiple small writes into larger, more efficient writes. This reduces the overhead of frequent disk accesses and improves overall throughput.