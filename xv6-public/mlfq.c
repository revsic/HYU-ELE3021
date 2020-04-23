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
stride_init(struct stride* this) {
  int i;

  this->total = 0;
  this->pass[0] = 0;
  this->ticket[0] = MAXTICKET;
  this->queue[0] = (struct proc*)-1;

  for (i = 1; i < NPROC; ++i) {
    this->pass[i] = -1;
    this->ticket[i] = 0;
    this->queue[i] = 0;
  }
}

int
stride_append(struct stride* this, struct proc* p, int usage) {
  int idx;
  float minpass;
  float* pass;
  struct proc** iter;

  if (this->total + usage > MAXSTRIDE)
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
  this->ticket[0] -= usage;
  this->ticket[idx] = usage;

  minpass = this->pass[0];
  for (pass = this->pass + 1; pass != &this->pass[NPROC]; ++pass) {
    if (*pass != -1 && minpass > *pass) {
      minpass = *pass;
    }
  }

  this->pass[idx] = minpass;
  return 1;
}

void
stride_delete(struct stride* this, struct proc* p) {
  int idx = p->mlfq.index;
  int usage = this->ticket[idx];
  this->total -= usage;
  this->ticket[0] += usage;

  this->pass[idx] = -1;
  this->ticket[idx] = 0;
  this->queue[idx] = 0;
}

inline int
stride_update_internal(struct stride* this, int idx) {
  float* pass;
  this->pass[idx] += (float)MAXTICKET / this->ticket[idx];
  if (this->pass[idx] > MAXPASS)
    for (pass = this->pass; pass != this->pass + NPROC; ++pass)
      if (*pass > 0)
        *pass -= MAXPASS - SCALEPASS;

  return MLFQ_NEXT;
}

int
stride_update(struct stride* this, struct proc* p) {
  return stride_update_internal(this, p->mlfq.index);
}

int
stride_update_mlfq(struct stride* this) {
  return stride_update_internal(this, 0);
}

struct proc*
stride_next(struct stride* this) {
  float* iter;
  float* pass = this->pass;
  float* minpass = this->pass;
  struct proc** queue = this->queue;

  for (iter = pass + 1; iter != &pass[NPROC]; ++iter)
    if (*iter != -1 && *minpass > *iter && queue[iter - pass]->state == RUNNABLE)
      minpass = iter;

  return queue[minpass - pass];
}

void
mlfq_init(struct mlfq* this)
{
  int i, j;
  struct proc** iter = &this->queue[0][0];

  static const uint quantum[] = { 1, 2, 4 };
  static const uint expire[] = { 5, 10, 100 };

  for (i = 0; i < NMLFQ; ++i) {
    this->quantum[i] = quantum[i];
    this->expire[i] = expire[i];
    for (j = 0; j < NPROC; ++j, ++iter)
      *iter = 0;
  }

  stride_init(&this->metasched);

  this->iter.level = 0;
  this->iter.iter = this->queue[0];
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
  int level = p->mlfq.level;
  int index = p->mlfq.index;
  if (!stride_append(&this->metasched, p, usage)) {
    return -1;
  }
  this->queue[level][index] = 0;
  return 0;
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

  stride_update_mlfq(&this->metasched);
  if (level + 1 < NMLFQ && p->mlfq.elapsed >= this->expire[level]) {
    if (mlfq_append(this, p, level + 1) != MLFQ_SUCCESS)
      panic("mlfq: level elevation failed");

    this->queue[level][index] = 0;
    return MLFQ_NEXT;
  }

  return MLFQ_KEEP;
}

struct proc*
mlfq_next(struct mlfq* this)
{
  int i;
  struct proc** iter;
  struct iterstate* state = &this->iter;

  int retry = 0;
  for (i = 0; i < NMLFQ; ++i) {
    if (i == state->level && !retry)
      iter = state->iter;
    else
      iter = this->queue[i];

    for (; iter != this->queue[i] + NPROC; ++iter) {
      if (*iter == 0 || (*iter)->state != RUNNABLE)
        continue;
      
      state->level = i;
      state->iter = iter;
      return *state->iter++;
    }

    if (i == state->level && !retry) {
      --i;
      retry = 1;
    }
    else
      retry = 0;
  }

  state->level = 0;
  state->iter = this->queue[0];
  return 0;
}

void
mlfq_boost(struct mlfq* this)
{
  int found;
  struct proc* p;
  struct proc** top = this->queue[0];
  struct proc** lower = this->queue[1];

  for (; lower != &this->queue[NMLFQ - 1][NPROC]; ++lower) {
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
  int keep;
  uint start, end, boost, boostunit;
  struct proc* p = 0;
  struct cpu* c = mycpu();
  struct stride* state = &this->metasched;

  c->proc = 0;
  boostunit = this->expire[NMLFQ - 1];

  keep = MLFQ_NEXT;
  boost = boostunit;
  for (;;) {
    sti();

    acquire(lock);
    do {
      if (keep == MLFQ_NEXT || p->state != RUNNABLE) {
        p = stride_next(state);
        if (p == (struct proc*)-1)
          p = mlfq_next(this);

        if (p == 0) {
          keep = stride_update_mlfq(state);
          break;
        }
      }

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      start = sys_uptime();
      swtch(&(c->scheduler), p->context);
      switchkvm();

      end = sys_uptime();
      p->mlfq.elapsed += end - start;
      keep = mlfq_update(this, p);

      if (end > boost) {
        mlfq_boost(this);
        boost += boostunit;
      }

      c->proc = 0;
    } while (0);
    release(lock);
  }
}
