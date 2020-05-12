# Thread

현재 xv6에는 thread 기능을 지원하고 있지 않다. 이에 Light-Weight Process (LWPs)를 구현하고, multi-threading을 지원한다.

## LWP: Light-Weight Process

기존의 process가 컴퓨터의 scheduling 단위를 추상화한 것이라면, thread는 실행 흐름을 추상화한 것이다. LWP는 thread의 구현을 위해 실행 흐름에 필요한 정보만을 축약하여 구성한 경량 프로세스 (Light-Weight Process)이다. 

process가 state를 관리한다면, thread는 flow를 관리하기 때문에 process로부터 pid, page directory와 VM, file descriptor 등을 공유하고, register context, stack, thread state, tid 등을 독립적으로 관리한다.

각각의 thread는 register context와 독립된 stack을 기반으로 instruction sequence를 실행해 나간다. 이때 vm을 공유하는 다른 스레드들과 race condition, deadlock 등의 문제가 발생할 수 있다.

## Design

**Thread sturct**

우선 [proc.h](./xv6-public/proc.h)의 proc structure를 수정한다. 기존의 proc은 [sz, pgdir, kstack, state, pid, parent, trapframe, context, chan, killed, ofile, cwd, name]로 구성되었다면, 실행 흐름에 관련된 부분들을 thread structure로 분리해낸다.

- struct proc: sz, pgdir, kstack, tf, context, pid, parent, killed, ofile, cwd, name, threadlist
- struct thread: state, tid, chan, context, user_thread

이후 scheduler에 의해 선택된 process는 thread 리스트를 순회하며 1tick씩 thread를 실행한다. context switch에서는 page directory와 VM을 공유하기 때문에 switchuvm, cr3 수정 없이 thread의 context를 kstack의 context에 복사하여 sched, 돌아와서는 kstack의 context를 thread에 복사한다. 이렇게 되면 TLB cache miss와 같은 vm exchange 관련 overhead를 줄일 수 있게 된다.

이렇게 state를 그대로 둠으로써 여타 syscall 등에서 발생할 수 있는 state synchronization 문제를 해결할 수 있을 것으로 보인다.

**Thread Creation: Method chaining**

exec에서는 proc->trap의 eip에 elf의 entry를 저장하고, esp에 새로 할당된 uservm을 저장한다. 동일하게 thread를 만든다면 context->eip에 trapret을 두고, sp를 [thread_func, helper, arg]로 둔다. 이렇게 되면 thread_func로 돌아간 후, 인자 자리에 arg가 할당되고, thread_func가 리턴된 후에는 helper가 실행된다.

**Thread Exit**

thread_helper는 리턴값을 가지고 thread structure의 retval에 저장한다. 이후 thread context를 정리하는 역할을 한다. state를 UNUSED로 바꾸고 user_thread에 retval를 기록한다.

exit에서는 강제 종료 요청이 들어왔을 때 하위 프로세스에 대해 직접적인 실행 종료를 강요하지 않고, 자식의 parent에 initproc을 할당하고 마친다. 마찬가지로 thread_exit에 의한 스레드 종료 요청이 들어오면, state를 ZOMBIE로 두고, helper에서 내용 정리를 마친다.

**Thread Joining**

메인 스레드를 SLEEP 상태로 두고, channel을 tid로 설정한다. 이후 helper에서 sibling threads 중 channel이 자신의 tid와 동일한 스레드를 wakeup 하는 방식으로 작동시킨다.

## Design 2

Design1을 구성한 후 추가 예정

## Implementation

- [ ] killed 처리
- [ ] proc lock 걸기
- [ ] allocproc, fork, userinit, wait, exit 등
- [ ] scheduler relative
- [ ] channel (sleep, wake) 처리
- [ ] state 처리
- [ ] syscall
