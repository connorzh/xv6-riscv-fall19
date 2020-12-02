// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct kmem
{
  struct spinlock lock;
  struct run *freelist;
};
struct kmem kmems[NCPU];
void kinit()
{
  int i;
  for (i = 0; i < NCPU; i++)
  {
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;
  int getTheId;
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;
  push_off();
  getTheId = cpuid();
  pop_off();
  acquire(&kmems[getTheId].lock);
  r->next = kmems[getTheId].freelist;
  kmems[getTheId].freelist = r;
  release(&kmems[getTheId].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct run *tmp;
  int getTheId;
  push_off();
  getTheId = cpuid();
  pop_off();
  acquire(&kmems[getTheId].lock);
  r = kmems[getTheId].freelist;
  if (r)
    kmems[getTheId].freelist = r->next;
  release(&kmems[getTheId].lock);
  if (!r)
  {
    for (int i = 0; i < NCPU; i++)
    {
      acquire(&kmems[i].lock);
    }
    for (int i = 0; i < NCPU; i++)
    {
      tmp = kmems[i].freelist;
      if (tmp)
      {
        r = kmems[i].freelist;
        if (r)
        {
          kmems[i].freelist = r->next;
          break;
        }
      }
    }
    for (int i = 0; i < NCPU; i++)
    {
      release(&kmems[i].lock);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
