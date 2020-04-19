struct mlfq {
  int num_queue;
  uint boost;
  uint quantum[NMLFQ];
  uint expire[NMLFQ];
  struct proc* queue[NMLFQ][NPROC];
  struct proc** current[NMLFQ];
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
};
