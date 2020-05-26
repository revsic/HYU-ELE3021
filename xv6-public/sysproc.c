#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_yield(void)
{
  yield();
  return 0;
}

// return MLFQ level of process.
int
sys_getlev(void)
{
  return getlev();
}

// move process form MLFQ scheduler to stride scheduler
// with given cpu usage.
int
sys_set_cpu_share(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;

  return set_cpu_share(n);
}

int
sys_thread_create(void)
{
  int *tid;
  void*(*start_routine)(void*);
  void *arg;

  if (argptr(0, (char**)&tid, sizeof tid) < 0)
    return -1;
  if (argptr(1, (char**)&start_routine, sizeof start_routine) < 0)
    return -1;
  if (argptr(2, (char**)&arg, sizeof arg) < 0)
    return -1;

  return thread_create(tid, start_routine, arg);
}

int
sys_thread_exit(void)
{
  void *retval;
  if (argptr(0, (char**)&retval, sizeof retval) < 0)
    return -1;
  
  thread_exit(retval);
  return 0;
}

int
sys_thread_join(void)
{
  int tid;
  void **retval;
  if (argint(0, &tid) < 0)
    return -1;
  if (argptr(1, (char**)&retval, sizeof retval) < 0)
    return -1;
  
  return thread_join(tid, retval);
}
