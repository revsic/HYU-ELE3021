/**
 *  This program runs child test programs concurrently.
 */

#include "types.h"
#include "stat.h"
#include "user.h"

// Number of child programs
#define CNT_CHILD           4

// Name of child test program that tests Stride scheduler
#define NAME_CHILD_STRIDE   "stridetests"
// Name of child test program that tests MLFQ scheduler
#define NAME_CHILD_MLFQ     "mlfqtests"

char *child_argv[CNT_CHILD][3] = {
  // Process scheduled by Stride scheduler, use 10% of CPU resources
  {NAME_CHILD_STRIDE, "10", 0},
  // Process scheduled by Stride scheduler, use 40% of CPU resources
  {NAME_CHILD_STRIDE, "40", 0},
  // Process scheduled by MLFQ scheduler, does not yield() itself
  {NAME_CHILD_MLFQ, "0", 0},
  // Process scheduled by MLFQ scheduler, frequently yield()
  {NAME_CHILD_MLFQ, "1", 0},
};

int
main(int argc, char *argv[])
{
  int pid;
  int i;

  for (i = 0; i < CNT_CHILD; i++) {
    pid = fork();
    if (pid > 0) {
      // parent
      continue;
    } else if (pid == 0) {
      // child
      exec(child_argv[i][0], child_argv[i]);
      printf(1, "exec failed!!\n");
      exit();
    } else {
      printf(1, "fork failed!!\n");
      exit();
    }
  }
  
  for (i = 0; i < CNT_CHILD; i++) {
    wait();
  }

  exit();
}
