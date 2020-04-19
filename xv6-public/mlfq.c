#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mlfq.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

extern int sys_uptime(void);

void
mlfq_init(struct mlfq* this, int num_queue, uint* rr, uint* expire)
{
  int i, j;
  struct proc** iter = &this->queue[0][0];

  this->num_queue = num_queue;
  for (i = 0; i < num_queue; ++i) {
    this->quantum[i] = rr[i];
    this->expire[i] = expire[i];

    for (j = 0; j < NPROC; ++j, ++iter)
      *iter = 0;
  }
}

void
mlfq_default(struct mlfq* this)
{
  static uint rr[] = { 1, 2, 4 };
  static uint expire[] = { 5, 10, 100 };
  mlfq_init(this, 3, rr, expire);
}

int
mlfq_append(struct mlfq* this, struct proc* p, int level)
{
  struct proc** iter;

  for (iter = this->queue[level]; iter != &this->queue[level][NPROC]; ++iter)
    if (*iter == 0)
      goto found;

  return MLFQ_FULL_QUEUE;

found:
  *iter = p;

  p->mlfq.level = level;
  p->mlfq.index = (iter - this->queue[level]) / sizeof iter;
  p->mlfq.elapsed = 0;
  return MLFQ_SUCCESS;
}

void
mlfq_delete(struct mlfq* this, struct proc* p)
{
  this->queue[p->mlfq.level][p->mlfq.index] = 0;  
}

int
mlfq_update(struct mlfq* this, struct proc* p)
{
  int result;
  int level = p->mlfq.level;
  int index = p->mlfq.index;

  if (p->state == ZOMBIE || p->killed)
    // queue is cleared in function wait.
    return MLFQ_NEXT;

  if (level + 1 < this->num_queue && p->mlfq.elapsed >= this->expire[level]) {
    result = mlfq_append(this, p, level + 1);
    if (result == MLFQ_SUCCESS) {
      this->queue[level][index] = 0;
      return MLFQ_NEXT;
    }

    return result;
  }

  return MLFQ_KEEP;
}

void
mlfq_scheduler(struct mlfq* this, struct spinlock* lock)
{
  int i, found, result;
  uint ticks;
  struct proc* p;
  struct proc** iter;
  struct cpu* c = mycpu();
  c->proc = 0;

  for (;;) {
    sti();

    acquire(lock);
    for (i = 0; i < this->num_queue; ++i) {
      found = 0;
      for (iter = this->queue[i]; iter != &this->queue[i][NCPU]; ++iter) {
        if ((*iter)->state != RUNNABLE)
          continue;

        found = 1;
        p = *iter;

        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        ticks = sys_uptime();
        swtch(&(c->scheduler), p->context);
        ticks = sys_uptime() - ticks;

        switchkvm();
        c->proc = 0;

        p->mlfq.elapsed += ticks;
        result = mlfq_update(this, p);
        if (result == MLFQ_KEEP)
          --iter;
        else if (result != MLFQ_NEXT)
          panic("error in mlfq.c:line125");
      }

      if (found)
        ++i;
    }
    release(lock);
  }
}