struct mlfq {
  int num_queue;
  uint quantum[NMLFQ];
  uint expire[NMLFQ];
  struct proc* queue[NMLFQ][NPROC];
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
};
