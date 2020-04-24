# 1.1 MLFQ + Stride scheduling

Design report에서 이야기하였던 MLFQ + Stride scheduler를 구현하였다.

아래에는 실제 구현과 예시 시나리오에 대한 실제 결과에 대해 분석한다.

## Controlling proportion between MLFQ and Stride

이번 프로젝트는 MLFQ Scheduler를 기본적으로 시행하되, 사용자의 요청이 있을 때 해당 비율만큼의 CPU 사용량을 프로세스에 할당할 수 있어야 한다. 이때 MLFQ Scheduler는 최소 20%의 CPU 사용량을 가져야 하며, 이 외 할당량을 직접 부여받은 프로세스의 사용량 총합은 80%를 넘지 않아야 한다.

Design report에서는 이를 하나의 MLFQ와 두 개의 Stride scheduler를 통해 관리할 것임을 이야기하였는데, 실제 구현에서는 하나의 MFLQ와 하나의 Stride scheduler를 통해 관리하였다.

커널의 동작을 분석하던 중 커널의 시작에서 다음과 같은 동작을 확인하였다. 

> main - userinit - allocproc

커널은 가장 먼저 `init` 프로세스를 생성하여 실행하는데, 이는 `ptable.proc`의 0번 칸을 고정적으로 점유하게 된다.

이에 아이디어를 착안하여 기본적으로 Stride scheduler를 두고, 큐의 0번 칸을 MLFQ Scheduler로 고정한다. 이후 CPU 할당량을 부여받은 프로세스는 1번칸부터 순서대로 할당 된다.

Master scheduler는 Stride scheduler에서 다음에 실행할 프로세스를 받고, 1번 칸 이후의 보통 프로세스는 context switch 후 실행, MLFQ scheduler를 받은 경우 MLFQ 에서 프로세스를 새로 받아 실행하게 된다.

Stride scheduler의 ticket 총량을 100으로 고정하고, 초기값으로 MLFQ에 100을 두면 처음에는 MLFQ 방식으로만 작동할 것이고, 할당량을 부여받은 프로세스가 도착하면 MLFQ의 ticket 수를 차감하는 방식으로 작동하여 총량 100%를 적절히 나눠 쓸 수 있게 된다.

```c
// Get next process from method to run.
struct proc* p = stride_next(state);
// If given process is MLFQ scheduler,
// request a new process.
if (p == MLFQ_PROC)
    p = mlfq_next(this);
```

## syscall: set_cpu_share

프로세스에 고정된 비율의 CPU 사용량을 부여하기 위해서는 부가적인 syscall이 필요하며, 과제 명세에서는 이를 `set_cpu_share`이라 하였다.

구현체에서는 `set_cpu_share`가 내부적으로 `mlfq_cpu_share`를 호출하며, 이는 MLFQ에서 해당 프로세스를 제거하고, stride scheduler로 올려보내는 역할을 한다.

```c
// in mlfq_cpu_share(mlfq.c)
// Append process to stride scheduler
if (!stride_append(&this->metasched, p, usage)) {
    return -1;
}
// Remove from MLFQ scheduler.
this->queue[level][index] = 0;
```

## Stride pass overflow

Stride scheduler에서 stride value의 타입을 어떻게 둘 것인지 고민하였다. 

int로 둘 경우에는 주어진 최대 63개 프로세스의 ticket에 대해 최소 공배수를 두고, 이를 개별 ticket으로 나눠야 정확한 비율의 stride value를 계산할 수 있다. 이의 수치가 적절한지 확인하기 위해 8개의 소수 [2, 3, 5, 7, 11, 13, 17, 19]를 티켓 수로 가정하면, 합이 77으로 80%를 넘지 않지만, 최소공배수가 9,699,690로 천만에 가까워진다. 이 경우 32bit에서는 19%에 해당하는 stride 510,510을 8천 번 가량 pass에 더하게 되면 overflow가 발생하고 꽤 잦은 scaling이 요구된다.

이를 해결하기 위해 stride의 타입을 float으로 가정하였다. 이 경우 실수 영역의 연산이 가능해져 stride를 단순 상수 C에 대해 [C / ticket]으로 둘 수 있게 된다. 이렇게 되면 적절히 작은 상수 C에 대해 [42억 / C * ticket]에 한번 overflow가 발생하고, scaling의 수가 줄어 overhead를 줄일 수 있다.

scaling 과정에는 ticket이 최소 1로, pass value 간 차이가 최대 [MAXTICKET / ticket = 100 / 1 = 100] 임을 감안하여 [MAXPASS - MAXTICKET]을 상수로 빼는 방식으로 구현하였다. 실질적 코드로는 이에 좀 더 여유를 두어 [MAXPASS - MAXTICKET * 100] 정도를 두었다.

```c
// If pass value exceeds maximum pass value,
// substract all pass value with predefined scaling term
// to maintain them in sufficient range.
if (this->pass[idx] > MAXPASS)
    for (pass = this->pass; pass != this->pass + NPROC; ++pass)
        if (*pass > 0)
            *pass -= MAXPASS - SCALEPASS;
```

## MLFQ Deadlock

만약 MLFQ에 할당된 프로세스가 init 뿐이고, shell의 종료를 wait하고 있을 때 MLFQ에 실행 가능한 프로세스는 없다. 이 경우 Stride scheduler에 등록된 프로세스는 MLFQ 보다 pass value가 크거나 같아지는 순간 CPU를 할당받지 못하고, MLFQ는 실행 가능한 프로세스가 없는 Deadlock에 놓이게 된다.

이를 해결하기 위해서 pass가 같은 경우에는 stride scheduler를 우선 할당하고, MLFQ에 실행 가능한 프로세스가 없더라도 우선 pass value를 업데이트하는 방식으로 이를 해결하였다.

```c
// Get next process from method to run.
p = stride_next(state);
// If given process is MLFQ scheduler,
// request a new process.
if (p == MLFQ_PROC)
    p = mlfq_next(this);

// If there is nothing runnable.
if (p == 0) {
    // Update MLFQ pass value for preventing deadlock.
    keep = stride_update_mlfq(state);
    break;
}
```

## MLFQ Iteration

MLFQ의 경우 상위 큐에 프로세스가 존재하면 우선 실행하고, 아닌 경우 현재 큐의 프로세스를 순서대로 실행해야 한다. 구현 과정에서 다음 프로세스를 가져오는 `mlfq_next`를 abstraction 하였기 때문에 iteration 과정에 현재 어떤 큐를 보고 있고, 어디까지 프로세스를 실행하였는지의 상태를 MLFQ가 별도로 보관하고 있어야 한다.

이를 `struct mlfq`에 `struct iterstate`를 두어 해결하였는데, mlfq_next에서는 toplevel 큐부터 순회를 시작하여 현재 큐에 도달하면 마지막으로 본 process 다음부터 진행하는 방식으로 구현하였다. `ptable.proc`의 크기가 [NPROC = 64]이고, MLFQ가 3xNPROC으로 구성되었기 때문에 프로세스를 검색할 떄마다 linear time이 소요되는 단점이 있지만 인덱싱 없이 iterator의 증감 연산을 이용한다는 점에서 어느 정도 tradeoff를 감안하였다.

## sys_uptime, cmostime

현재 커널에는 시간을 확인하는 방식이 두 가지 존재한다. 실행 시점부터 10ms 단위로 정수형 변수를 증가하는 tick과 cmos에서 시간을 읽어오는 cmostime이다. 현재는 tick-based로 프로세스의 실행 시간을 측정하는데, 이 경우 10ms을 채우지 않고 yield를 하는 경우 프로세스가 toplevel 큐에 상존하게 하는 adversarial attack이 가능하다.

cmostime의 경우에는 최소 시간 단위가 초(sec)이기 때문에 더 작은 단위의 측정이 어렵다. 이를 해결하기 위해서는 더 작은 시간 단위가 필요로 하고, 레포트 작성 이후에 더 찾아볼 예정이다.

## Tick checking after context switch

현재는 process가 context switching을 한 후에 scheduler 단계에서 tick 수를 세어 queue의 이동을 결정한다. 이 경우 동일한 프로세스가 다시 올라가더라도 switching overhead를 그대로 들고 가기 때문에 MLFQ의 policy에 따른 benefit을 얻을 수는 있지만 optimal한 솔루션은 아니다. 이후에 trap.c 에서 yield를 하기 전에 tick을 검사하는 방식을 도입할 예정이다.

## Analysis

아래에서는 실제 구현체에 대한 결과를 분석해본다.

### MLFQ Analysis

프로세스가 고정된 CPU 할당량을 받지 않은 경우에는 MLFQ 방식으로만 작동한다. 이 경우 loop를 돌면서 각 레벨에 있던 시간을 측정하면 expire time의 비율과 유사한 tick 수를 얻어야 한다.

|      | highest | middle | lowest | 
| ---- | ------- | ------ | ------ |
|  RR  | 1 tick  | 2 tick | 4 tick |
| Expire | 5 tick | 10 tick |

현재 설정에서는 1:2 정도를 얻어야 하는 것이다.

```
$ mlfqtests 0
MLFQ(compute), lev[0]: 1186, lev[1]: 2495, lev[2]: 16320
$ mlfqtests 1
MLFQ(yield), lev[0]: 1007, lev[1]: 2401, lev[2]: 16593
```

실제로 1186:2495, 1007:2401로 대략 1:2 정도의 실험 결과를 확인하였다.

|      | highest | middle | lowest | 
| ---- | ------- | ------ | ------ |
|  RR  | 1 tick  | 2 tick | 4 tick |
| Expire | 5 tick | 20 tick |

설정을 바꿔 1:4가 나오는 것도 확인하였다.

```
init: starting sh
$ mlfqtests 0
MLFQ(compute), lev[0]: 1259, lev[1]: 5019, lev[2]: 13723
$ mlfqtests 1
MLFQ(yield), lev[0]: 1064, lev[1]: 4380, lev[2]: 14557
```

### Stride Analysis


### Master Analysis
