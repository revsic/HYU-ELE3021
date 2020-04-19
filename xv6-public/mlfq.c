void
mlfq_init(struct mlfq* this, int num_queue, uint* rr, uint* expire, uint boost)
{
  int i, j;
  struct proc** iter = this->queue;

  this->num_queue = num_queue;
  this->boost = boost;

  for (i = 0; i < num_queue; ++i) {
    this->quantum[i] = rr[i];
    this->expire[i] = expire[i];

    for (j = 0; j < NPROC; ++j, ++iter)
      *iter = NULL;

    this->current[i] = NULL;
  }
  return 0;
}

int
mlfq_default(struct mlfq* this)
{
  static uint rr[] = { 1, 2, 4 };
  static uint expire[] = { 5, 10, ~(1 << 31) };
  return mlfq_init(this, 3, rr, expire, 100);
}

int
mlfq_append(struct mlfq* this, struct proc* p)
{
  struct proc** iter;

  for (iter = this->queue[0]; iter != &this->queue[0][NPROC]; ++iter)
    if (*iter == NULL)
      goto found;

  return MLFQ_FULL_QUEUE;

found:
  *iter = p;

  p->mlfq.level = 0;
  p->mlfq.index = (iter - this->queue[0]) / sizeof(iter);
  p->mlfq.elapsed = 0;

  if (this->current[0] == NULL) {
    this->current[0] = iter;
  }
  return MLFQ_SUCCESS;
}

struct proc*
mlfq_top(struct mlfq* this)
{
  struct proc*** iter;
  for (iter = this->current; iter != &this->current[this->num_queue]; ++iter)
    if (*iter != NULL)
      return **iter;
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