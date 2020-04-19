#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mlfq.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

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
mlfq_append(struct mlfq* this, struct proc* p)
{
  struct proc** iter;

  for (iter = this->queue[0]; iter != &this->queue[0][NPROC]; ++iter)
    if (*iter == 0)
      goto found;

  return MLFQ_FULL_QUEUE;

found:
  *iter = p;

  p->mlfq.level = 0;
  p->mlfq.index = (iter - this->queue[0]) / sizeof(iter);
  p->mlfq.elapsed = 0;
  return MLFQ_SUCCESS;
}

int
mlfq_update(struct mlfq* this, struct proc* p, int done)
{
  return 0;
}

void
mlfq_scheduler(struct mlfq* this, struct spinlock* lock)
{
  int i, found;
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

        swtch(&(c->scheduler), p->context);
        switchkvm();

        c->proc = 0;
      }

      if (found) {
        --i;
      }
    }
    release(lock);
  }
}