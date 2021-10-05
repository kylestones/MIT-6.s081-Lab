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
  struct spinlock lock;
  struct run *freelist;

} kmem;

struct {
  struct spinlock refcntlock;
  uint refcnt[(PHYSTOP - KERNBASE)  / PGSIZE];
} kmemrefcnt;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  initlock(&kmemrefcnt.refcntlock, "kmemrefcnt");
  memset(kmemrefcnt.refcnt, 0, sizeof(kmemrefcnt.refcnt));
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquirememrefcntlock();
  submemrefcnt((uint64)pa);
  if (getmemrefcnt((uint64)pa) > 0) {
    releasememrefcntlock();
    return;
  }
  releasememrefcntlock();

  //printf("%s-%d:pa=%p, refcnt=%d\n", __FUNCTION__, __LINE__, pa, getmemrefcnt((uint64)pa));
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // TODO 这里为什么不需要锁保护？虽然会有死锁，但是还是感觉有问题呀？
    //acquirememrefcntlock();
    setmemrefcnt ((uint64)r, 1);
    //releasememrefcntlock();
  }

  return (void*)r;
}

void
acquirememrefcntlock()
{
  acquire(&kmemrefcnt.refcntlock);
}

void
releasememrefcntlock()
{
  release(&kmemrefcnt.refcntlock);
}

// 获取物理内存的引用次数
uint
getmemrefcnt (uint64 pa)
{
  if (pa >= PHYSTOP || pa < KERNBASE)
  {
    return 0;
  }

  uint refcnt = kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)];

  return refcnt;
}

// 物理内存的引用次数加加
// return -1 表示失败
int
addmemrefcnt (uint64 pa)
{
  if (pa >= PHYSTOP || pa < KERNBASE) {
    return -1;
  }

  if (kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)] == 0xffffffff) {
    return -1;
  }

  kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)] += 1;

  return 0;
}

// 物理内存的引用次数减减
// return -1 表示失败
int
submemrefcnt (uint64 pa)
{
  if (pa >= PHYSTOP || pa < KERNBASE) {
    return -1;
  }

  if (kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)] == 0x0) {
    return -1;
  }

  kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)] -= 1;

  return 0;
}

// 设置物理内存的引用次数
// return -1 表示失败
int
setmemrefcnt (uint64 pa, uint cnt)
{
  if (pa >= PHYSTOP || pa < KERNBASE) {
    return -1;
  }

  kmemrefcnt.refcnt[PYMEMREFCNTINDEX(pa)] = cnt;

  return 0;
}
