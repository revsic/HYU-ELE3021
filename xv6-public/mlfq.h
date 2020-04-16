struct mlfq {
  int num_queue;
  int sizes[NMLFQ];
  struct proc* queue[NMLFQ][NPROC];
  unsigned long long quantum[NMLFQ];
  unsigned long long expire[NMLFQ];
  unsigned long long boost;
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
};
