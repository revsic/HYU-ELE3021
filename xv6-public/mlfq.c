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

// Initialize stride scheduler.
// First process is MLFQ scheduler.
// Function mlfq_cpu_share moves a process to the stride scheduler,
// and it place in the queue at index after 0.
// It controlls the proportion between stride scheduling process and
// MFLQ scheduling process.
void
stride_init(struct stride* this) {
  int i;
  // Initialize MLFQ scheduler
  this->total = 0;
  this->pass[0] = 0;
  this->ticket[0] = MAXTICKET;
  this->queue[0] = (struct proc*)-1;

  // Make queue empty except MLFQ scheduler.
  for (i = 1; i < NPROC; ++i) {
    this->pass[i] = -1;
    this->ticket[i] = 0;
    this->queue[i] = 0;
  }
}

// Append process to the stride scheduler with given proportion of cpu usage.
int
stride_append(struct stride* this, struct proc* p, int usage) {
  int idx;
  float minpass;
  float* pass;
  struct proc** iter;
  // If total proprotion exceeds maximum stride scheduling.
  if (this->total + usage > MAXSTRIDE)
    return 0;

  // Find empty space.
  for (iter = this->queue; iter != &this->queue[NPROC]; ++iter)
    if (*iter == 0)
      goto found;

  return 0;

found:
  idx = iter - this->queue;
  // Set scheduler information in process.
  p->mlfq.level = -1;
  p->mlfq.index = idx;

  *iter = p;
  this->total += usage;
  this->ticket[0] -= usage;
  this->ticket[idx] = usage;

  // Set pass value of given process
  // with minimum pass value between existing processes.
  minpass = this->pass[0];
  for (pass = this->pass + 1; pass != &this->pass[NPROC]; ++pass) {
    if (*pass != -1 && minpass > *pass) {
      minpass = *pass;
    }
  }

  this->pass[idx] = minpass;
  return 1;
}

// Delete process from stride scheduler.
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

// Update pass value of process with given queue index.
inline int
stride_update_internal(struct stride* this, int idx) {
  float* pass;
  this->pass[idx] += (float)MAXTICKET / this->ticket[idx];

  // If pass value exceeds maximum pass value,
  // substract all pass value with predefined scaling term
  // to maintain them in sufficient range.
  if (this->pass[idx] > MAXPASS)
    for (pass = this->pass; pass != this->pass + NPROC; ++pass)
      if (*pass > 0)
        *pass -= MAXPASS - SCALEPASS;

  return MLFQ_NEXT;
}

// Update pass value of given process.
int
stride_update(struct stride* this, struct proc* p) {
  return stride_update_internal(this, p->mlfq.index);
}

// Update pass value of MLFQ scheduler.
int
stride_update_mlfq(struct stride* this) {
  return stride_update_internal(this, 0);
}

// Get next process based on stride scheduling policy.
struct proc*
stride_next(struct stride* this) {
  float* iter;
  float* pass = this->pass;
  float* minpass = this->pass;
  struct proc** queue = this->queue;

  // Get process which is runnable and have minimum pass value.
  for (iter = pass + 1; iter != &pass[NPROC]; ++iter)
    if (*iter != -1 && *minpass > *iter && queue[iter - pass]->state == RUNNABLE)
      minpass = iter;

  return queue[minpass - pass];
}

// Initialize MLFQ scheduler. 
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

  // Stride scehduler acts as meta-scheduler,
  // which controls the cpu usage between MLFQ scheduling process
  // and stride scheduling process.
  stride_init(&this->metasched);

  // Initialize iterator state to start with first process, initproc.
  this->iter.level = 0;
  this->iter.iter = this->queue[0];
}

// Append process to MLFQ scheduler.
int
mlfq_append(struct mlfq* this, struct proc* p, int level)
{
  struct proc** iter;
  // Find empty space.
  for (iter = this->queue[level]; iter != &this->queue[level][NPROC]; ++iter)
    if (*iter == 0)
      goto found;

  return MLFQ_FULL_QUEUE;

found:
  *iter = p;
  // Update scheduler information of given process.
  p->mlfq.level = level;
  p->mlfq.index = iter - this->queue[level];
  p->mlfq.elapsed = 0;
  return MLFQ_SUCCESS;
}

// Pass process to the stride scheduler.
int
mlfq_cpu_share(struct mlfq* this, struct proc* p, int usage)
{
  int level = p->mlfq.level;
  int index = p->mlfq.index;
  if (!stride_append(&this->metasched, p, usage)) {
    return -1;
  }
  // Remove from MLFQ scheduler.
  this->queue[level][index] = 0;
  return 0;
}

// Delete process from MLFQ scheduler.
void
mlfq_delete(struct mlfq* this, struct proc* p)
{
  // If process level is set to -1,
  // it indicates that process is scheduled by stride scheduler.
  if (p->mlfq.level == -1)
    stride_delete(&this->metasched, p);
  else
    this->queue[p->mlfq.level][p->mlfq.index] = 0;  
}

// Update process level by checking elapsed time.
int
mlfq_update(struct mlfq* this, struct proc* p)
{
  int level = p->mlfq.level;
  int index = p->mlfq.index;

  // When process terminated, queue is cleared by method wait().
  if (p->state == ZOMBIE || p->killed)
    return MLFQ_NEXT;

  // If process level is -1, it indicates scheduled by stride scheduler.
  if (level == -1)
    return stride_update(&this->metasched, p);

  // Update pass value of MLFQ scheulder.
  stride_update_mlfq(&this->metasched);
  // If avilable time is expired, move the process to the next queue.
  if (level + 1 < NMLFQ && p->mlfq.elapsed >= this->expire[level]) {
    if (mlfq_append(this, p, level + 1) != MLFQ_SUCCESS)
      panic("mlfq: level elevation failed");

    // Remove from current level queue.
    this->queue[level][index] = 0;
    return MLFQ_NEXT;
  }

  return MLFQ_KEEP;
}

// Get next process with MLFQ scheduling policy.
// If it returns zero, it means nothing runnaable.
struct proc*
mlfq_next(struct mlfq* this)
{
  int i;
  struct proc** iter;
  struct iterstate* state = &this->iter;

  int retry = 0;
  for (i = 0; i < NMLFQ; ++i) {
    // Starting with top level,
    // if the iterator arrived at the level of last iteration,
    // starts from the next process of last returned
    // and if there is no process runnable,
    // re-iterate from the first process of current level.
    if (i == state->level && !retry)
      iter = state->iter;
    else
      iter = this->queue[i];

    for (; iter != this->queue[i] + NPROC; ++iter) {
      // Just runnable process.
      if (*iter == 0 || (*iter)->state != RUNNABLE)
        continue;
      
      // Update iterator state and return process.
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

  // Make state indicates first process, initproc.
  state->level = 0;
  state->iter = this->queue[0];
  // Nothing to runnable.
  return 0;
}

// Boost all process to the top level.
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
    // Find empty space on top level.
    for (; top != &this->queue[0][NPROC]; ++top) {
      if (*top != 0)
        continue;
      
      // Move lower proceee to the top level.
      *top = *lower;
      *lower = 0;
      // Update scheduler information.
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

// MLFQ scheduler.
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
    // Enable interrupts.
    sti();

    acquire(lock);
    do {
      // If previous run commands replace the proc or
      // current process is not runnable.
      if (keep == MLFQ_NEXT || p->state != RUNNABLE) {
        // Get next process from method to run.
        p = stride_next(state);
        // If given process is MLFQ scheduler,
        // request a process to it.
        if (p == (struct proc*)-1)
          p = mlfq_next(this);

        // If there is nothing runnable.
        if (p == 0) {
          // Update MLFQ pass value for preventing deadlock.
          keep = stride_update_mlfq(state);
          break;
        }
      }

      // Switch to chosen process.
      // It is the process's job to relase ptable.lock
      // and then reacquire it before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      start = sys_uptime();
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Update MLFQ states.
      end = sys_uptime();
      p->mlfq.elapsed += end - start;
      keep = mlfq_update(this, p);

      // If boosting time arrived.
      if (end > boost) {
        mlfq_boost(this);
        boost += boostunit;
      }

      c->proc = 0;
    } while (0);
    release(lock);
  }
}
