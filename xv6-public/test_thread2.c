#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 10
#define NTEST 14

// Show race condition
int racingtest(void);

// Test basic usage of thread_create, thread_join, thread_exit
int basictest(void);
int jointest1(void);
int jointest2(void);

// Test whether a process can reuse the thread stack
int stresstest(void);

// Test what happen when some threads exit while the others are alive
int exittest1(void);
int exittest2(void);

// Test fork system call in multi-threaded environment
int forktest(void);

// Test exec system call in multi-threaded environment
int exectest(void);

// Test what happen when threads requests sbrk concurrently
int sbrktest(void);

// Test what happen when threads kill itself
int killtest(void);

// Test pipe is worked in multi-threaded environment
int pipetest(void);

// Test sleep system call in multi-threaded environment
int sleeptest(void);

// Test behavior when we use cpu_share with thread
int stridetest(void);

volatile int gcnt;
int gpipe[2];

int (*testfunc[NTEST])(void) = {
  racingtest,
  basictest,
  jointest1,
  jointest2,
  stresstest,
  exittest1,
  exittest2,
  forktest,
  exectest,
  sbrktest,
  killtest,
  pipetest,
  sleeptest,
  stridetest,
};
char *testname[NTEST] = {
  "racingtest",
  "basictest",
  "jointest1",
  "jointest2",
  "stresstest",
  "exittest1",
  "exittest2",
  "forktest",
  "exectest",
  "sbrktest",
  "killtest",
  "pipetest",
  "sleeptest",
  "stridetest",
};

int
main(int argc, char *argv[])
{
  int i;
  int ret;
  int pid;
  int start = 0;
  int end = NTEST-1;
  if (argc >= 2)
    start = atoi(argv[1]);
  if (argc >= 3)
    end = atoi(argv[2]);

  for (i = start; i <= end; i++){
    printf(1,"%d. %s start\n", i, testname[i]);
    if (pipe(gpipe) < 0){
      printf(1,"pipe panic\n");
      exit();
    }
    ret = 0;

    if ((pid = fork()) < 0){
      printf(1,"fork panic\n");
      exit();
    }
    if (pid == 0){
      close(gpipe[0]);
      ret = testfunc[i]();
      write(gpipe[1], (char*)&ret, sizeof(ret));
      close(gpipe[1]);
      exit();
    } else{
      close(gpipe[1]);
      if (wait() == -1 || read(gpipe[0], (char*)&ret, sizeof(ret)) == -1 || ret != 0){
        printf(1,"%d. %s panic\n", i, testname[i]);
        exit();
      }
      close(gpipe[0]);
    }
    printf(1,"%d. %s finish\n", i, testname[i]);
    sleep(100);
  }
  exit();
}

// ============================================================================
void nop(){ }

void*
racingthreadmain(void *arg)
{
  int tid = (int) arg;
  int i;
  int tmp;
  for (i = 0; i < 10000000; i++){
    tmp = gcnt;
    tmp++;
	asm volatile("call %P0"::"i"(nop));
    gcnt = tmp;
  }
  thread_exit((void *)(tid+1));

  return 0;
}

int
racingtest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  gcnt = 0;
  
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], racingthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0 || (int)retval != i+1){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"%d\n", gcnt);
  return 0;
}

// ============================================================================
void*
basicthreadmain(void *arg)
{
  int tid = (int) arg;
  int i;
  for (i = 0; i < 100000000; i++){
    if (i % 20000000 == 0){
      printf(1, "%d", tid);
    }
  }
  thread_exit((void *)(tid+1));

  return 0;
}

int
basictest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], basicthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0 || (int)retval != i+1){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

// ============================================================================

void*
jointhreadmain(void *arg)
{
  int val = (int)arg;
  sleep(200);
  printf(1, "thread_exit...\n");
  thread_exit((void *)(val*2));

  return 0;
}

int
jointest1(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_create(&threads[i-1], jointhreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  printf(1, "thread_join!!!\n");
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_join(threads[i-1], &retval) != 0 || (int)retval != i * 2 ){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

int
jointest2(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;
  
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_create(&threads[i-1], jointhreadmain, (void*)(i)) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  sleep(500);
  printf(1, "thread_join!!!\n");
  for (i = 1; i <= NUM_THREAD; i++){
    if (thread_join(threads[i-1], &retval) != 0 || (int)retval != i * 2 ){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1,"\n");
  return 0;
}

// ============================================================================

void*
stressthreadmain(void *arg)
{
  thread_exit(0);

  return 0;
}

int
stresstest(void)
{
  const int nstress = 35000;
  thread_t threads[NUM_THREAD];
  int i, n;
  void *retval;

  for (n = 1; n <= nstress; n++){
    if (n % 1000 == 0)
      printf(1, "%d\n", n);
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], stressthreadmain, (void*)i) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
      }
    }
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_join(threads[i], &retval) != 0){
        printf(1, "panic at thread_join\n");
        return -1;
      }
    }
  }
  printf(1, "\n");
  return 0;
}

// ============================================================================

void*
exitthreadmain(void *arg)
{
  int i;
  if ((int)arg == 1){
    while(1){
      printf(1, "thread_exit ...\n");
      for (i = 0; i < 5000000; i++);
    }
  } else if ((int)arg == 2){
    exit();
  }
  thread_exit(0);

  return 0;
}

int
exittest1(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], exitthreadmain, (void*)1) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  sleep(1);
  return 0;
}

int
exittest2(void)
{
  thread_t threads[NUM_THREAD];
  int i;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], exitthreadmain, (void*)2) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  while(1);
  return 0;
}

// ============================================================================

void*
forkthreadmain(void *arg)
{
  int pid;
  if ((pid = fork()) == -1){
    printf(1, "panic at fork in forktest\n");
    exit();
  } else if (pid == 0){
    printf(1, "child\n");
    exit();
  } else{
    printf(1, "parent\n");
    if (wait() == -1){
      printf(1, "panic at wait in forktest\n");
      exit();
    }
  }
  thread_exit(0);

  return 0;
}

int
forktest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], forkthreadmain, (void*)0) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  return 0;
}

// ============================================================================

void*
execthreadmain(void *arg)
{
  char *args[3] = {"echo", "echo is executed!", 0}; 
  sleep(1);
  exec("echo", args);

  printf(1, "panic at execthreadmain\n");
  exit();
}

int
exectest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], execthreadmain, (void*)0) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1, "panic at exectest\n");
  return 0;
}

// ============================================================================

void*
sbrkthreadmain(void *arg)
{
  int tid = (int)arg;
  char *oldbrk;
  char *end;
  char *c;
  oldbrk = sbrk(1000);
  end = oldbrk + 1000;
  for (c = oldbrk; c < end; c++){
    *c = tid+1;
  }
  sleep(1);
  for (c = oldbrk; c < end; c++){
    if (*c != tid+1){
      printf(1, "panic at sbrkthreadmain\n");
      exit();
    }
  }
  thread_exit(0);

  return 0;
}

int
sbrktest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], sbrkthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }

  return 0;
}

// ============================================================================

void*
killthreadmain(void *arg)
{
  kill(getpid());
  while(1);
}

int
killtest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], killthreadmain, (void*)i) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  while(1);
  return 0;
}

// ============================================================================

void*
pipethreadmain(void *arg)
{
  int type = ((int*)arg)[0];
  int *fd = (int*)arg+1;
  int i;
  int input;
  for (i = -5; i <= 5; i++){
    if (type){
      read(fd[0], &input, sizeof(int));
      __sync_fetch_and_add(&gcnt, input);
      //gcnt += input;
    } else{
      write(fd[1], &i, sizeof(int));
    }
  }
  thread_exit(0);

  return 0;
}

int
pipetest(void)
{
  thread_t threads[NUM_THREAD];
  int arg[3];
  int fd[2];
  int i;
  void *retval;
  int pid;

  if (pipe(fd) < 0){
    printf(1, "panic at pipe in pipetest\n");
    return -1;
  }
  arg[1] = fd[0];
  arg[2] = fd[1];
  if ((pid = fork()) < 0){
      printf(1, "panic at fork in pipetest\n");
      return -1;
  } else if (pid == 0){
    close(fd[0]);
    arg[0] = 0;
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], pipethreadmain, (void*)arg) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
      }
    }
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_join(threads[i], &retval) != 0){
        printf(1, "panic at thread_join\n");
        return -1;
      }
    }
    close(fd[1]);
    exit();
  } else{
    close(fd[1]);
    arg[0] = 1;
    gcnt = 0;
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], pipethreadmain, (void*)arg) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
      }
    }
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_join(threads[i], &retval) != 0){
        printf(1, "panic at thread_join\n");
        return -1;
      }
    }
    close(fd[0]);
  }
  if (wait() == -1){
    printf(1, "panic at wait in pipetest\n");
    return -1;
  }
  if (gcnt != 0)
    printf(1,"panic at validation in pipetest : %d\n", gcnt);

  return 0;
}

// ============================================================================

void*
sleepthreadmain(void *arg)
{
  sleep(1000000);
  thread_exit(0);

  return 0;
}

int
sleeptest(void)
{
  thread_t threads[NUM_THREAD];
  int i;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], sleepthreadmain, (void*)i) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
    }
  }
  sleep(10);
  return 0;
}

// ============================================================================

void*
stridethreadmain(void *arg)
{
  int *flag = (int*)arg;
  int t;
  while(*flag){
    while(*flag == 1){
      for (t = 0; t < 5; t++);
      __sync_fetch_and_add(&gcnt, 1);
    }
  }
  thread_exit(0);

  return 0;
}

int
stridetest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  int pid;
  int flag;
  void *retval;

  gcnt = 0;
  flag = 2;
  if ((pid = fork()) == -1){
    printf(1, "panic at fork in stridetest\n");
    exit();
  } else if (pid == 0){
    set_cpu_share(2);
  } else{
    set_cpu_share(10);
  }

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], stridethreadmain, (void*)&flag) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  flag = 1;
  sleep(500);
  flag = 0;
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }

  if (pid == 0){
    printf(1, " 2% : %d\n", gcnt);
    exit();
  } else{
    printf(1, "10% : %d\n", gcnt);
    if (wait() == -1){
      printf(1, "panic at wait in stridetest\n");
      exit();
    }
  }

  return 0;
}

// ============================================================================
