struct stride {
  uint total;
  float pass[NPROC];
  uint ticket[NPROC];
  struct proc* queue[NPROC];
};

struct mlfq {
  int num_queue;
  uint quantum[NMLFQ];
  uint expire[NMLFQ];
  struct proc* queue[NMLFQ][NPROC];
  struct stride metasched;

  struct iterstate {
    int level;
    struct proc** iter;
  } iter;
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
  MLFQ_NEXT = 2,
  MLFQ_KEEP = 3,
};
