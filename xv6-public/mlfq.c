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
  p->mlfq.index = iter - this->queue[level];
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
mlfq_boost(struct mlfq* this)
{
  int found;
  struct proc* p;
  struct proc** top = this->queue[0];
  struct proc** lower = this->queue[1];

  for (; lower != &this->queue[this->num_queue - 1][NPROC]; ++lower) {
    if (*lower == 0)
      continue;

    found = 0;
    for (; top != &this->queue[0][NPROC]; ++top) {
      if (*top != 0)
        continue;
      
      *top = *lower;
      *lower = 0;

      p = *top;
      cprintf("boost %d %d -> 0 %d\n", p->mlfq.level, p->mlfq.index, top - this->queue[0]);
      
      p->mlfq.level = 0;
      p->mlfq.index = top - this->queue[0];
      p->mlfq.elapsed = 0;

      found = 1;
      break;
    }

    if (!found)
      panic("mlfq boost: could not find empty space of toplevel queue");
  }
}

void
mlfq_scheduler(struct mlfq* this, struct spinlock* lock)
{
  int i, found;
  uint start, end, boost;
  struct proc* p;
  struct proc** iter;
  struct cpu* c = mycpu();
  c->proc = 0;

  boost = this->expire[2];
  for (;;) {
    sti();

    acquire(lock);
    for (i = 0; i < this->num_queue;) {
      found = 0;
      for (iter = this->queue[i]; iter != &this->queue[i][NCPU]; ++iter) {
        if (*iter == 0 || (*iter)->state != RUNNABLE)
          continue;

        found = 1;
        p = *iter;

        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        start = sys_uptime();
        swtch(&(c->scheduler), p->context);
        switchkvm();

        end = sys_uptime();
        p->mlfq.elapsed += end - start;
        if (mlfq_update(this, p) == MLFQ_KEEP)
          --iter;

        if (end > boost) {
          mlfq_boost(this);
          boost += this->expire[2];
        }

        c->proc = 0;
      }

      if (found)
        i = 0;
      else
        ++i;
    }
    release(lock);
  }
}