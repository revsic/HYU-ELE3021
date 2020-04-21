# 1. CPU Scheduling

첫 과제는 xv6의 CPU scheduling 기법을 변경하는 것이다.

명세에 따르면 MLFQ + Stride scheduling 방식으로, MLFQ scheduler 위에서 프로세스를 실행하다 `set_cpu_share` syscall이 발생하면 Stride scheduler로 변경하는 것이다. 

## xv6 - RoundRobin

기존의 xv6 scheduler를 보면 다음과 같다.

1. kernel (main.c) main - mpmain - scheduler 순으로 호출되면, scheduler는 무한 루프를 돌면서 runnable 한 process를 찾아 나간다. 
2. Runnable 프로세스를 찾으면 현재 `struct cpu`의 proc 멤버를 해당 프로세스의 포인터로 대체하고, scheduler 멤버를 old context로  swtch 함수를 호출한다.
3. swtch 함수는 old context로 입력 들어온 scheduler 멤버를 현재 kernel stack의 esp로 덮어쓰고, new context로 cpu register를 변조한다. 
4. swtch의 retn을 통해 해당 프로세스의 next instruction을 실행해 나간다.

10ms 이후 timer interrupt가 발생하면 vectors.S - alltrap (trapasm.S) - trap (trap.c)이 호출되며, trap 함수는 현재 프로세스가 running state일 때 yield 함수를 호출해 `struct cpu`의 scheduler 멤버로 context switch, cpu의 권한이 다시 kernel의 scheduler 함수로 넘어간다. 

이를 따라 보면 xv6는 별다른 scheduling 메소드 없이 10ms timer interrupt가 발생할 때마다 context switch를 호출하고 있었고, 이는 Round-Robin, quantum=10ms 방식에 따라 cpu를 time-sharing하고 있던 것이다.

## MLFQ: Multi-Level Feedback Queue

MLFQ는 turnaround time과 response time을 적절히 trade-off 하기 위해 만들어진 스케줄링 기법이다.

turnaround time을 optimizing 하기 위해서는 SJF(Shortest Job First) 혹은 STCF (Shortest Time-to-Completion First) 기법을 사용해야 하지만, 어떤 job이 가장 빨리 끝날지 알 수 없으므로 MLFQ에서는 과거 정보를 tracing 하여 얼마나 걸릴지 추정하고, 이를 기반으로 각 job에 priority를 설정, priority가 같은 job을 하나의 queue에 보관한다.

response time을 optimizing 하기 위해서는 Round-Robin 기법이 적절하므로 각각의 queue 내부 job에 대해서는 RR 기법을 통해 scheduling 한다.

MLFQ는 priority가 높은 queue의 job이 모두 소진될 때까지 하위 queue의 job을 실행할 수 없으며, 상위 queue의 job이 정해진 시간만큼의 cpu 자원을 할당받았음에도 작업을 끝내지 못하면 하위 queue로 이동한다.

이때 하위 queue의 starvation을 방지하기 위해 일정 시간마다 하위 queue의 프로세스를 최상위 queue로 다시 올리는데, 이를 priority boosting이라고 한다.

명세에서는 1tick=10ms를 기준으로 3개의 priority queue에 대해 다음과 같은 시간 기준을 설정하였다.

|      | highest | middle | lowest | 
| ---- | ------- | ------ | ------ |
|  RR  | 1 tick  | 2 tick | 4 tick |
| Expire | 5 tick | 10 tick |

이때 priority boosting은 100 tick마다 진행한다.

## Stride Scheduling

Propositional Share Scheduling은 프로세서가 원하는 만큼 공평히 사용할 수 있게 하기 위한 스케줄링 기법이다. 시스템 리소스를 tickets이라는 개념으로 지분화 하여 전체에서 그 비율만큼 프로세스에 cpu 자원이 할당되도록 한다.

Stride scheduling 또한 이 기법의 일종으로, deterministic 하게 cpu 자원이 적절히 할당되도록 한다.

Stride scheduling에서는 stride라는 개념을 정의하는데, [stride = (total tickets) / (own tickets)]로 두고, queue에서 pass value가 가장 작은 job을 꺼내 고정된 time quantum만큼 실행 후, pass value에 stride만큼 더해 가는 방식으로 진행된다. 

즉 ticket 수가 많은 job은 stride가 작아져 같은 pass value를 갖기 위해 많은 time-quantum이 할당될 것이고, ticket 수가 적은 job은 stride가 커져 같은 pass value를 갖기 위해 한 두 번의 time-quantum만을 할당받아도 충분할 것이다.

이를 활용하여 Stride scheduling은 원하는 proportion만큼 cpu 자원을 할당받을 수 있게 된다.

## MLFQ + Stride Scheduling

명세에서는 프로세스가 생성되면 MLFQ를 통해 scheduling하고, 특정 syscall을 호출한 후에는 stride scheduling 방식으로 scheduling 할 것을 요구하였다. 

이때 MLFQ는 최소 20%의 CPU 사용량을 유지해야 하고, Stride scheduling은 최대 80%의 CPU 사용량을 넘어설 수 없다.

이에 stride scheduler를 1차로 두고, 여기에 mlfq token만 두었다가, 추후에 stride scheduler에 job이 추가되면 stride token을 추가한다. 이를 최대 2:8로 두고, 스케줄링 전에 mlfq token이 나오면 mlfq 방식으로, stride token이 나오면 stride 방식으로 스케줄링 할 것이다.

이후에는 mlfq scheduler와 2차 stride scheduler를 두어 실질적인 job scheduling에 이용하여 본 명세를 구현할 수 있을 듯하다.

## Abstraction

struct queue (queue.c): FIFO 구조체

- queue_init(struct queue*): queue 초기화
- queue_push(struct queue*, T elem): element 추가 
- queue_pop(struct queue*) -> T: element 삭제

struct pque (pque.c): Priority Queue

- pque_init(struct pque*): pque 초기화
- pque_top(struct pque*) -> T: 최고 priority 객체
- pque_push(struct pque*, T elem): element 추가
- pque_pop(struct pque*): top element 삭제

struct mlfq (mlfq.c): MLFQ 구조체

- mlfq_init(struct mlfq*, int num_queue, int* rr, int* expire, int boost): mlfq 초기화
- mlfq_append(struct mlfq*, struct proc*): runnable process 추가
- mlfq_top(struct mlfq*) -> struct proc*: highest priority process.
- mlfq_update(struct mlfq*, int done) -> highest priority process의 tick count을 추가하고 priority를 적절히 조정

struct strided (strided.c): stride scheduler

- stride_init(struct strided*): stride 초기화
- stride_append(struct strided*, struct proc*, int ticket): 새 프로세서 추가
- stride_top(struct strided*) -> struct proc*: top process.
- stride_update(struct strided*, int done) -> top process의 tickcount 추가, 순서 조정.

syscall

- yield: cpu 양보
- getlev: 현재 프로세스의 mlfq level 반환
- set_cpu_share: stride scheduler로 변경

```c
/// in set_cpu_share
stride_append(strided, p, n_ticket);
// TODO: stride token update
```

```c
// in scheduler
struct proc* token = stride_top(stride_token);
stride_update(stride_token, FALSE);

if (token == MLFQ_TOKEN) {
    p = mlfq_top(mlfq);
    swtch(&cpu->scheduler, p);
    mlfq_update(mlfq, p->state == DONE);
} else {
    p = stride_top(strided);
    swtch(&cpu_scheduler, p);
    stride_update(strided, p->state == DONE);
}
```

## Implementation

proc.c에 따르면 ptable.proc에 값을 쓰는건 allocproc 함수에서
=> allocproc에서 scheduler에 등록하면 바로 확인 가능.

**push down vs iteration**

1. linked list
2. array (push down: move all elem)
3. array (state-base: UNUSED) -> proc table

proc.c에서 ptable.proc의 iteration 과정을 보면 c++의 배열에 정의된 std::begin, end와 동일히 작동. indexing이 아닌 pointer의 증가연산을 통해 작동. 컴파일러 최적화 수준마다 다르겠지만, 두 선택지에 대한 operation 갯수가 indexing에서 더 많아짐. 

ptable.proc에서는 여기에 push down 없이 단순 UNUSED slot에 추가하는 방식. 이 때 scheduler는 approximated RR이 되는데, 이 이유는 프로세스들이 생성 순으로 proc array에 저장되는 것이 아닌, proc array의 빈공간에 순서대로 채워지기 때문임.

mlfq의 경우에도 사실 이처럼 작동해도 무관함. 그럴 경우 mlfq의 array가 struct { state, proc* }로 구성되어야 하고, 이럴 경우에는 elapsed time을 proc에서 mlfq로 이동해도 됨.
- 장점: proc 4byte 경량화, xv6 coding style에 맞음
- 단점: mlfq struct가 무거워짐, append 내부 search 과정에서 배열 순회 요구 O(N = 64)

***반면에 push down approach가 이뤄질 경우***
- 장점: indexing 단순화
- 단점: append, update 내부 update 과정에서 배열 순회 요구 O(N = 64)

approximated push down (current index)
- 장점: pushdown 없이 iteration으로 real RR 구현이 가능함
- 단점: current indices를 담을 array, 반복 루틴이 2개가 필요 (ring 형태)

**TODO: cache hit**

현재 mflq는 proc*[NMLFQ][NPROC]으로 구성되어 있는데, ptable.proc에서는 iteration이 연속된 메모리에서 일어나 cache hit가 용이한 대신, mlfq는 포인터로 비연속적인 공간을 linked list와 같이 탐색하기 때문에 cache hit가 어려움. 확인 요구

**ptable lock**

현재 scheduler는 lock 상태에서 context switch를 하는데, 이래도 multiprocessor 환경에서 병목이 일어나지 않는 이유는, [yield ->] sched -> swtch 순서로 context switch가 일어나는데, 이 때 sched는 lock이 걸려 있는지 확인하고, yield에서는 lock이 걸린채로 이뤄진 context switch 후에 release를 하고, user procedure로 넘어가기 때문.

**process 생애주기**

allocproc에 의해 process 생성, ptable에 접근하여 UNUSED proc에 대해 새로 process를 구성함. state=EMBYRO, pid=nextpid++로 두고 context를 초기화, rop로 forkret과 trapret을 리턴하게 구성함. fork 등은 allocproc으로 만든 proc 구조체에 uvm을 복사하여 사용하는 형식.

kernel은 userinit을 통해 initproc을 만드는데, init은 fork(child) - exec(shell) - exec(proc)를 통해 새로운 process를 자식 프로세스로 두고, fork(parent) - wait loop을 통해 자식 프로세스의 종료를 정리한다.

exit은 자식 프로세스와 본인의 proc state를 ZOMBIE로 두고, 자식 프로세스의 parent를 init으로 두어 프로세스가 모두 init 내 wait을 통해 정리될 수 있게 한다. wait은 자식 프로세스 중 ZOMBIE인 모든 프로세스의 자원을 free하고 state를 UNUSED로 변경한다. 

즉 프로세스의 생성은 allocproc에서, 종료는 wait에서 확인할 수 있다. 또한, yield된 프로세스는 scheduler로 context switch가 발생하므로 scheduler를 통해 확인할 수 있다.

**tick**

trap.c를 보면 `tickslock: struct spinlock`과 `ticks: uint`가 있는데, 이는 timer interrupt가 발생할 떄 마다 tick을 증가시키고, 해당 tick을 channel로 가지고 있는 모든 process를 runnable하게 한다.

즉 tick은 운영체제가 시작한 후 매 10ms 마다 1씩 증가하는 정수인 것이고, 32bit OS에서 42억까지 표현 가능하므로 대략 486일 정도에 한번 overflow가 발생한다. uptime syscall은 이 tick을 반환하는 역할을 한다.

proc.c의 sleep은 channel을 tick으로, state를 SLEEPING으로 설정한 후 sched를 호출하는데, sleep syscall은 proc의 sleep을 활용하여 주어진 시간 이상이 지날 때 까지 sleep을 계속 호출한다.

**kill**

proc.c의 kill은 myproc을 직접 정리하지 않고 p->killed=1 & p->state=runnable 상태로 만드는데, 이에 대해 exit을 호출하는 주체는 trap이다. syscall이나 timer interrupt가 들어왔을 때 해당 프로세스가 user 영역에 있으면 exit을 force해버린다.

**TODO**

- [x] MLFQ without expire
- [x] mlfq_append to allocproc
- [x] tick (elapsed)
- [x] mlfq_update
- [x] mlfq_update to scheduler, wait 
- [x] expire
- [x] boost
- [x] getlev
- [ ] do not switch when inefficient rr ticks
- [ ] tick base -> cmostime based

- [ ] stride scheduling
- [ ] mixing
- [ ] set cpu share

**appendix**

process state 수정: allocproc, userinit, fork, exit, wait, scheduler, yield, sleep, wakeup1, kill
ptable 접근: pinit, allocproc, userinit, fork, exit, wait, scheduler, yield, forkret, sleep, wakeup1, wakeup, kill, procdump

- pinit: lock initialization
- allocproc: proc struct 설정하고, ptable의 unused 공간에 할당
    - acquire, release, iteration
    - ptable unused proc -> EMBRYO, pid alloc
- userinit: initproc 생성
    - acquire, release
    - initproc->state = RUNNABLE
- fork: process duplication
    - acquire, release, iteration
    - np->state = RUNNABLE
- exit: process termination, 프로세스를 직접 정리하지 않고, zombie로 둔 다음에 부모에서 wait을 통해 하위 프로세스를 정리하는 방식.
    - acquire -> wakeup1(parent)
    - child->parent = initproc, child->state == ZOMBIE -> wakeup1(initproc)
    - curproc->state = ZOMBIE, sched
- wait: wait for child process
    - acquire, release, iteration
    - p->parent == curproc & p->state == ZOMBIE -> process 정리 (p->state = UNUSED)
- scheduler: process scheduling
    - acquire, release, iteration
    - p->state = RUNNING
- yield: give up cpu owning
    - acquire, release
    - myproc()->state = RUNNABLE
- forkret: forked child return address
    - release
- sleep: assign channel to current proc, change state to sleeping, sched
    - acquire, release
    - p->chan = chan, p->state = SLEEPING
- wakup1: wakup all proc relative to given channel
    - iteration
- wakeup: locked version of wakup1
    - acquire, release, iteration
- kill: make process killed flag on, make state runnable if sleeping
    - acquire, release, iteration
