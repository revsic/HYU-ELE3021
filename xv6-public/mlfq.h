struct mlfq {
  int num_queue;
  int sizes[NMLFQ];
  struct proc* queue[NMLFQ][NPROC];
  uint quantum[NMLFQ];
  uint expire[NMLFQ];
  uint boost;
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
};
