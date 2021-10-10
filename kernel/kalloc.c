// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
#define LOCK_NAME_MAX_LEN 6

  int cpuindex;
  char lockname[LOCK_NAME_MAX_LEN];
  for (cpuindex = 0; cpuindex < NCPU; cpuindex++) {
    snprintf(lockname, LOCK_NAME_MAX_LEN, "%s%d", "kmem", cpuindex);
    initlock(&kmem.lock[cpuindex], lockname);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cid;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  cid = cpuid();
  acquire(&kmem.lock[cid]);
  r->next = kmem.freelist[cid];
  kmem.freelist[cid] = r;
  release(&kmem.lock[cid]);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();

  int cid = cpuid();
  acquire(&kmem.lock[cid]);
  r = kmem.freelist[cid];
  if(r)
    kmem.freelist[cid] = r->next;
  release(&kmem.lock[cid]);

  pop_off();

  if (!r)  {
    for (int cindex = 0; cindex < NCPU; cindex++) {
      acquire(&kmem.lock[cindex]);
      r = kmem.freelist[cindex];
      if(r) {
        kmem.freelist[cindex] = r->next;
        release(&kmem.lock[cindex]);
        break;
      }
      release(&kmem.lock[cindex]);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
