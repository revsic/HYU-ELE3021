# Thread - Implementation

이번 문서에서는 마스터 브랜치 커밋 [64284e2](/commit/64284e2e40101fb51154e392d0679d21d9fc3a1b)을 기준으로 xv6에 Thread를 어떻게 구현하였고, 어떤 문제들이 있었는지 이야기한다.

## 1. Implementation

실제 구현에 사용된 스레드는 Milestone 1에서 이야기한 디자인과는 조금 다른 부분이 존재한다. 이를 간단히 짚고 넘어가고자 한다.

**Structure**

기본적인 틀은 일전의 디자인과 유사하다. proc에서 control flow를 담당하는 상태를 분리해서 thread를 구성하고, proc이 사전에 `NTHREAD`만큼의 UNUSED thread 구조체를 미리 가지고 있다. 

아래는 실제 스레드 구조체이다. 달라진 점이 있다면, 기존에는 state, tid, chan, context 정도를 control flow를 위한 상태로 분리하였지만, 실제 thread의 정상적인 scheduling을 위해서는 timer interrupt가 발생하고 trapframe과 context를 kernel stack에 개별적으로 보관하고 있어야 했기 때문에 해당 메모리 영역 또한 thread 마다 개별적으로 할당한 것이다.

```c
// Per-thread state
struct thread {
  enum procstate state;         // thread state
  int tid;                      // thread ID
  void *chan;                   // if non-zero, sleeping on chan
  char *kstack;                 // bottom of kernel stack
  struct trapframe *tf;         // trap frame for current interrupt handler.
  struct context *context;      // cpu context, swtch() here to run process
  void* retval;                 // return value
};
```

프로세스 구조체에는 4가지 추가 필드를 선언하였다. 현재 프로세스에서 어떤 thread가 동작 중인지를 나타내는 thread index `tidx`, 프로세스에서 수용할 수 있는 thread 집합인 `threads`, 각각의 thread에 할당할 kernel stack `kstacks`와 user stack `ustacks`이다.

kstacks와 ustacks는 한번 `thread_create`에 의해 할당되면 프로세스가 `wait`에 의해 정리될 때까지 남아 있다가, thread가 할당되면 memory pool로써 적절한 스택 공간을 제공한다. 이는 하나의 프로세스에서 대단히 많은 스레드를 생성하고 종료할 때 생기는 상당량의 memory allocation 부하를 막기 위함이다.

```c
// Per-process state
struct proc {
  // ... 
  int tidx;                         // index of running thread
  struct thread threads[NTHREAD];   // thread pool
  char* kstacks[NTHREAD];           // kernel stack pool
  uint ustacks[NTHREAD];            // user stack pool
  // ...
};
```

**Stack construction**

`allocproc`이나 `fork`, `exec`를 참고하면 프로세스를 새로 생성하고 적절한 entry point에서 instruction flow를 시작하기 위해 kernel stack을 적절히 조작한다. 

참고할 예로, scheduling 과정에서는 timer interrupt가 발생하면 [vectors - trapasm - trap - yield]의 순서로 scheduler와 context switch가 발생하는데, 이 과정에서 kernel stack은 [trapframe | trapret | local stacks | context]와 유사한 모양을 띤다.

이는 scheduler에 의해서 cpu가 할당되는 과정에 [swtch(context-eip) - local returns - trapret(trapframe-eip)] 순으로 총 (2+n)-번의 return 과정을 거쳐 원래의 instruction flow로 돌아온다. allocproc 에서는 이 과정을 묘사하기 위해 context-eip를 forkret으로, 적절한 위치에 trapret을 기록하였고, fork는 원래의 flow로 돌아가기 위해 trapframe을 원본에서 복사해오기도 하였다.

thread creation에서는 새로 만들어진 Light-weight Process (LWPs)가 사용자에 의해 주어진 start routine을 시작하기 위해 마찬가지로 kernel stack을 조작해야 했다. allocproc과 유사히 context-eip를 forkret으로, trapret을 적절한 위치에 적어 두었고, tf-eip를 start routine의 주소로 두었다.

그뿐만 아니라 user stack을 할당하여 start routine이 정상 작동할 수 있는 독립된 공간을 분리해야 했고, start routine에 void* 타입의 인자를 넘겨주기 위해서 user stack에도 추가적인 작업이 필요로 했다. user stack은 [arg | retaddr]로 구성하였고, retaddr은 형식상 thread_epilogue로 채워두었다. 물론 user-level에서 kernel 함수를 호출할 수 없기 때문에, `thread_exit`함수를 호출하지 않는 이상 kernel crash가 발생하면서 스레드는 비정상 종료하게 된다.

**Thread exit, epilogue**

일전의 디자인에서는 `thread_helper`라는 부가 함수를 도입하여 retval을 기록하고 thread context를 정리하는 역할을 할 것이라 이야기하였다. 이와 비슷한 실제 구현체가 `thread_epilogue`이다.

thread_epilogue는 현재 thread의 state를 ZOMBIE로 두고, 자신의 tid를 channel로 sleep하고 있는 다른 스레드, 즉 자신을 join하고 있는 다른 thread를 wakeup한 후 scheduler에게 cpu 권한을 이양한다. 직접 thread를 정리하지 않고 exit과 같이 단순 zombie 처리를 한 이유는, thread에 저장된 return value를 thread_join이 받아 기록해야 했기 때문이다. 아니라면 process에 retval를 기록하는 구조체가 따로 필요할 것이다.

thread_exit은 return value를 thread의 retval field에 저장하고 thread_epilogue를 불러 정상 종료시키는 간단한 함수이다.

**Thread joining**

thread join은 tid를 받아 대상 thread를 검색하고, 해당 thread가 ZOMBIE, 정상 종료된 상태가 아니라면 tid를 channel로 sleep 한다. 앞서 이야기하였듯 thread_epilogue는 thread의 상태를 단순 zombie로 바꾸고 tid channel을 wakeup하는 역할을 한다. thread_join은 이후 return value를 기록하고, state와 kernel stack을 초기화함으로써 thread의 life cycle이 마무리된다.

**Scheduling**

실제 scheduler는 process 내부에 thread가 존재하도록 구현하였기 때문에 수정할 부분이 많지 않았고, 단순히 process의 runnable 여부를 runnable 한 thread가 존재하는지, 존재한다면 몇 번 index에 있는지 (proc->tidx) 정도만 수정하였다.

thread 사이의 switching은 user vm을 공유하고 있으므로 cr3 레지스터를 초기화할 필요 없이, 단순 interrupt가 발생했을 때 loading 할 kernel stack의 주소만 변경하면 된다. 이는 cpu의 Task State Segment (TSS)의 esp0에서 관리하며, 이를 스케치한 함수는 다음과 같다.

```c
// vm.c
void
switch_trap_kstack(struct proc *p)
{
  pushcli();
  struct thread *t = &p->threads[p->tidx];
  // switch default kernel stack to current thread.
  mycpu()->ts.esp0 = (uint)t->kstack + KSTACKSIZE;
  popcli();
}
```

이를 토대로 trap에서는 `mlfq_yieldable` 함수를 통해 현재 tick count가 time quantum을 넘었는지 확인하고, 넘었다면 scheduler로 context switching, 안 넘었다면 `next_thread` 함수를 통해 다음 runnable thread를 검색하고, [switch_trap_kstack - swtch]를 통해 간단한 switching을 진행한다.

## 2. Test result

전체 스크린샷은 [rsrc](./rsrc)를 확인한다.

0. racingtest

Racing test는 공유 자원 `gcnt`를 두고 10개의 스레드가 동시에 접근해서 increase operation을 진행했을 때 data race 문제로 정상적인 sum이 나오지 않음을 확인하는 문제이다.

```
0. racingtest start
25178248
0. racingtest finish
```

실제로 10개 스레드 1억번의 increase 결과 턱없이 적은 숫자가 나온 것을 확인할 수 있었다.

1. basictest

Basic test는 thread의 생성과 종료, join 과정에 다른 오류가 없었는지, return value는 적절히 전달되었는지를 확인한다.

```
1. basictest start
01234256789013424013567892134056789123405678958967
1. basictest finish
```

2. jointtest1

첫 번째 Join test는 test 내부에 [sleep - thread_exit] 페어를 통해 종료 시각을 늦추고, join이 먼저 호출되어 정상적으로 sleep, wakeup이 가능하지 확인한다.

```
2. jointest1 start
thread_join!!!
thread_exit...
thread_exit...
thread_exit...
thread_exit...
thread_exit...
thread_exit...
thread_exthread_exit...
it...
thread_exit...
thread_exit...

2. jointest1 finish
```

3. jointest2

두 번쨰 join test는 main thread에서 thread보다 더 오랜 sleep을 통해 join의 호출 시점을 늦추고, 이미 종료된 thread에 대해서 정상적인 behavior를 보여주는지 확인한다.

```
3. jointest2 start
thread_exit...
thread_exit...
thread_exit...
thread_exit...
thread_exit...
thread_exit...
threthread_exit...
ad_exit...
thread_exit...
thread_exit...
thread_join!!!

3. jointest2 finish
```

4. stresstest

Stress test는 최대 10개 thread를 35000번 생성, 종료했을 때 memory allocation 등 별다른 오류 없이 정상 작동하는지 확인하는 테스트이다.

```
4. stresstest start
1000
2000
3000
4000
// ...
33000
34000
35000

4. stresstest finish
```

5. exittest1

첫 번째 exit test는 메인 스레드가 종료되었을 때 다른 스레드가 같이 종료되는지를 확인하는 테스트이다. 명확한 exit 없이 종료된다면 자식 스레드는 모두 zombie가 되어 무한 대기 상태일 것이라 여겨 exit을 추가하였지만, 간헐적으로 무한히 기다리는 것으로 보아 추가 실험 및 보수가 필요하다.

```
5. exittest1 start
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
tthread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
thread_exit ...
QEMU: Terminated
```

6. exittest2

두 번째 exit test는 thread에서 exit을 호출하였을 때 모든 LWP가 종료되는지 확인하는 테스트이다.

```
6. exittest2 start
6. exittest2 finish
```

7. forktest

Fork test는 스레드 내부에서 fork를 호출하였을 때, 해당 프로세스에서 uvm을 복사하고, 해당 thread의 instruction flow를 따라 실행해 갈 수 있는지 확인한다. 

테스트에서는 parent와 child를 출력하도록 해두었으니, 10개 스레드에서 각각 10번의 parent와 child 호출이 있었는지 확인한다.

```
7. forktest start
parent
child
parent
child
parent
child
parent
child
parent
child
parent
child
parent
child
parent
child
parent
child
parent
child
7. forktest finish
```

8. exectest

Exec test는 child thread에서 exec를 호출하였을 떄 전체 LWP가 정리되고, 하나의 LWP에서만 exec가 호출되는지 확인한다.

테스트에서는 echo를 실행해 보았다. 10개의 스레드에서 호출하였어도 하나의 echo만 실행되어야 한다.

```
8. exectest start
echo is executed!
8. exectest finish
```

9. sbrktest

Sbrk 테스트는 10개의 스레드가 동시에 uvm 확장을 요청했을 때, serializable한 action이 진행되었는지 확인한다.

```
9. sbrktest start
9. sbrktest finish
```

10. killtest

Kill test는 스레드에서 kill이 호출되었을 때 관련 스레드를 모두 정리하고 프로세스가 정상 종료되는지 확인한다.

```
10. killtest start
10. killtest finish
```

11. pipetest

Pipe test는 스레드 환경에서도 pipe가 duplication이나 lose가 발생하지 않고 정상 작동하는지 확인한다.

```
11. pipetest start
11. pipetest finish
```

12. sleeptest

Sleep test는 main thread보다 오래 sleep 하는 thread에 대해 main thread가 종료되었을 때 같이 종료되는지 확인한다.

```
12. sleeptest start
12. sleeptest finish
```

13. stridetest 

Stride test는 stride scheduling 된 프로세스에서 실행된 thread도 같은 비중의 cpu share를 받는지 확인한다.

```
13. stridetest start
40% : 773998806
30% : 538293580
13. stridetest finish
```

## 3. Problems

**Page fault**

[proc.c](./xv6-public/proc.c)의 `next_thread`에서 기존의 scheduler와 같이 `switchkvm`을 swtch 후에 두었는데, 이것이 CR3를 업데이트하고, `switchuvm` 없이 `switch_trap_kstack`만을 호출하여 thread switching을 진행하다 보니 user page를 page directory에서 찾을 수 없어 page fault가 발생하였음. 

[wiki.osdev.org](https://wiki.osdev.org/Exceptions#Page_Fault)를 참고하여 page fault의 error code를 확인하고, call stack을 따라 `write` syscall 호출부에서 발생함을 알게 되어, kvm으로의 switching 없이 바로 thread switching을 하여 고칠 수 있었음.

**Double acquire**

stack pool이 없는 상황에서 stresstest를 진행하다 보니 user memory allocation이 안되는 순간이 왔음. 하지만 acquire 한 ptable lock을 `thread_create`에서는 release 없이 return을 하였고, panic이 터지면서 `consolewrite` 함수에서 호출한 `wakeup`이 ptable lock을 다시 acquire 하려다 커널에 크래쉬가 발생함.

일차적인 해결을 위해 `thread_create`의 모든 return 부분에 release ptable lock을 추가함.

**Kernel, user stack pool**

stack 공간을 위한 user memory allocation은 `allocuvm`함수를 이용하였는데, 이 함수는 단방향으로 작동하여 중간의 일정 메모리를 해제하고 할당하는 것이 불가능함. 이에 uvm의 할당 해제 없이 스레드가 생길 때마다 stack 공간을 할당하였고, stresstest에서 memory allocation에 실패함. 

이를 해결하기 위해 process에 kernel, user stack pool을 준비하고, 한번 할당되면 프로세스가 종료될 때까지 남아 있으면서 thread가 생성될 때마다 1page씩 할당해 주는 역할을 함. 이를 통해 메모리를 재사용하고, 최대 NTHREAD 이상의 할당이 일어나지 않게 됨.
