/**
 *  This program requests portion of CPU resources with given parameter
 * value by calling set_cpu_share() system call.
 *  After that, periodically increases cnt values until its LIFETIME.
 */

#include "types.h"
#include "stat.h"
#include "user.h"

#define LIFETIME        1000        // (ticks)
#define COUNT_PERIOD    1000000     // (iteration)

int
main(int argc, char *argv[])
{
  uint i;
  int cnt = 0;
  int cpu_share;
  uint start_tick;
  uint curr_tick;

  if (argc < 2) {
    printf(1, "usage: sched_test_stride cpu_share(%)\n");
    exit();
  }

  cpu_share = atoi(argv[1]);

  // Register this process to the Stride scheduler
  if (set_cpu_share(cpu_share) < 0) {
    printf(1, "cannot set cpu share\n");
    exit();
  }

  // Get start time
  start_tick = uptime();

  i = 0;
  while (1) {
    i++;

    // Prevent code optimization
    __sync_synchronize();

    if (i == COUNT_PERIOD) {
      cnt++;

      // Get current time
      curr_tick = uptime();

      if (curr_tick - start_tick > LIFETIME) {
        // Terminate process
        printf(1, "STRIDE(%d%%), cnt: %d\n", cpu_share, cnt);
        break;
      }
      i = 0;
    }
  }

  exit();
}
