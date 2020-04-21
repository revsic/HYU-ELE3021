/**
 *  This program periodically counts the level(priority) of queue of the
 * MLFQ scheduler which is containing this process.
 *  Periodically yield if given parameter value is 1
 *  Do not yield itself if given parameter value is 0
 */

#include "types.h"
#include "stat.h"
#include "user.h"

#define LIFETIME        200000000   // (iteration)
#define YIELD_PERIOD    10000       // (iteration)

// Number of level(priority) of MLFQ scheduler
#define MLFQ_LEVEL      3

int
main(int argc, char *argv[])
{
    uint i;
    int cnt_level[MLFQ_LEVEL] = {0, 0, 0};
    int do_yield;
    int curr_mlfq_level;

    if (argc < 2) {
        printf(1, "usage: sched_test_mlfq do_yield_or_not(0|1)\n");
        exit();
    }

    do_yield = atoi(argv[1]);

    i = 0;
    while (1) {
        i++;
        
        // Prevent code optimization
        __sync_synchronize();

        if (i % YIELD_PERIOD == 0) {
            // Get current MLFQ level(priority) of this process
            curr_mlfq_level = getlev();
            cnt_level[curr_mlfq_level]++;

            if (i > LIFETIME) {
                printf(1, "MLFQ(%s), lev[0]: %d, lev[1]: %d, lev[2]: %d\n",
                        do_yield==0 ? "compute" : "yield",
                        cnt_level[0], cnt_level[1], cnt_level[2]);
                break;
            }

            if (do_yield) {
                // Yield process itself, not by timer interrupt
                yield();
            }
        }
    }

    exit();
}
