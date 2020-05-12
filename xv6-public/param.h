#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks

#define NMLFQ         3  // number of multi-level feedback queue.
#define MAXTICKET   100  // maximum number of ticket.
#define MAXSTRIDE    80  // maximum number of stride tickets.
#define MAXPASS      10000000  // maximum number of pass.
#define SCALEPASS    100000  // scaling pass.

#define NTHREAD       8  // maximum number of threads.
