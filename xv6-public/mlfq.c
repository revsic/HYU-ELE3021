void
mlfq_init(struct mlfq* this, int num_queue, uint* rr, uint* expire, uint boost)
{
  int i;
  this->num_queue = num_queue;
  for (i = 0; i < num_queue; ++i) {
    this->sizes[i] = 0;
    this->quantum[i] = rr[i];
    this->expire[i] = expire[i];
  }
  this->boost = boost;
  return 0;
}

int
mlfq_default(struct mlfq* this)
{
  uint rr[] = { 1, 2, 4 };
  uint expire[] = { 5, 10, ~(1 << 31) };
  return mlfq_init(this, 3, rr, expire, 100);
}

int
mlfq_append(struct mlfq* this, struct proc* p)
{
  p->elapsed = 0;
  if (this->sizes[0] >= NPROC) {
    return MLFQ_FULL_QUEUE;
  }

  this->queue[0][this->sizes[0]] = p;
  
  p->sched.quelevel = 0;
  p->sched.queindex = this->sizes[0];
  p->sched.elapsed = 0;

  this->sizes[0]++;
  return MLFQ_SUCCESS;
}

struct proc*
mlfq_top(struct mlfq* this)
{
  if (this->sizes[0] > 0)
    return this->queue[0][0];
  if (this->sizes[1] > 0)
    return this->queue[1][0];
  if (this->sizes[2] > 0)
    return this->queue[2][0];
  return NULL;
}

int
mlfq_update(struct mlfq* this, struct proc* p, int done)
{
  
}

void
mlfq_scheduler(struct mlfq* this, struct spinlock* lock)
{
  
}