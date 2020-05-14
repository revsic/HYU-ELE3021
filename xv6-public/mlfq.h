// Stride scheduler context
struct stride {
  uint quantum;
  uint total;                 // total proportion of stride scheduling process
  float pass[NPROC];          // pass values, sum of inverse ticket
  uint ticket[NPROC];         // proportion of stride scheduling process
  struct proc* queue[NPROC];  // process queue
};

// MLFQ scheduler context
struct mlfq {
  uint quantum[NPROC];                // round robin time quantum
  uint expire[NPROC];                 // time to downgrade level
  struct proc* queue[NMLFQ][NPROC];   // process queue
  struct stride metasched;            // meta-scheduler for controlling proportion
  struct proc** iterstate[NMLFQ];     // iterator state
};

enum mlfqstate {
  MLFQ_SUCCESS = 0,
  MLFQ_FULL_QUEUE = 1,
  MLFQ_NEXT = 2,
  MLFQ_KEEP = 3,
};
