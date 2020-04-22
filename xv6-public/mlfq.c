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
stride_init(struct stride* this, int maxima) {
  int i;

  this->maxima = maxima;
  this->total = 0;

  for (i = 0; i < NPROC; ++i) {
    this->pass[i] = 0;
    this->ticket[i] = 0;
    this->queue[i] = 0;
  }

  this->ticket[0] = MAXTICKET - this->maxima;
  this->queue[0] = (struct proc*)-1;
}

int
stride_append(struct stride* this, struct proc* p, int usage) {
  int idx;
  float minpass;
  float* pass;
  struct proc** iter;

  if (this->total + usage > this->maxima)
    return 0;

  for (iter = this->queue; iter != &this->queue[NPROC]; ++iter)
    if (*iter == 0)
      goto found;

  return 0;

found:
  idx = iter - this->queue;
  p->mlfq.level = -1;
  p->mlfq.index = idx;

  *iter = p;
  this->total += usage;
  this->ticket[idx] = usage;

  minpass = 0;
  for (pass = this->pass; pass != &this->pass[NPROC]; ++pass) {
    if (*pass != 0 && *pass > minpass) {
      minpass = *pass;
    }
  }

  this->pass[idx] = minpass;
  return 1;
}

void
stride_delete(struct stride* this, struct proc* p) {
  this->queue[p->mlfq.index] = 0;
}

int
stride_update(struct stride* this, struct proc* p) {
  int idx = p->mlfq.index;
  this->pass[idx] += (float)MAXTICKET / this->ticket[idx];
  return MLFQ_NEXT;
}

void
mlfq_init(struct mlfq* this, int maxmeta, int num_queue, uint* rr, uint* expire)
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

  stride_init(&this->metasched, maxmeta);
}

void
mlfq_default(struct mlfq* this)
{
  static uint rr[] = { 1, 2, 4 };
  static uint expire[] = { 5, 10, 100 };
  mlfq_init(this, MAXSTRIDE, 3, rr, expire);
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

int
mlfq_cpu_share(struct mlfq* this, struct proc* p, int usage)
{
  if (!stride_append(&this->metasched, p, usage)) {
    return 0;
  }
  mlfq_delete(this, p);
  return 1;
}

void
mlfq_delete(struct mlfq* this, struct proc* p)
{
  if (p->mlfq.level == -1)
    stride_delete(&this->metasched, p);
  else
    this->queue[p->mlfq.level][p->mlfq.index] = 0;  
}

int
mlfq_update(struct mlfq* this, struct proc* p)
{
  int level = p->mlfq.level;
  int index = p->mlfq.index;

  if (p->state == ZOMBIE || p->killed)
    // queue is cleared in function wait.
    return MLFQ_NEXT;

  if (level == -1)
    return stride_update(&this->metasched, p);

  if (level + 1 < this->num_queue && p->mlfq.elapsed >= this->expire[level]) {
    if (mlfq_append(this, p, level + 1) != MLFQ_SUCCESS)
      panic("mlfq: level elevation failed");

    this->queue[level][index] = 0;
    return MLFQ_NEXT;
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