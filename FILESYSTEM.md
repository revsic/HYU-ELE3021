# Filesystem

이번 문서에서는 xv6에서 Filesystem을 어떻게 구성하고 있고, 이의 기능을 어떻게 확장할 수 있었는지에 대해 다룬다.

## 0. xv6 Filesystem

xv6에서는 disk를 superblock과 bitmap, disk inode, blocks와 logs로 구성하고 있다.

```c
// super block describes the disk layout:
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};
```

가장 먼저 나오는 superblock은 xv6의 filesystem의 총괄적인 정보를 담고 있다. 몇 개의 block과 inode가 있는지, log와 inode, bitmap은 어느 offset부터 시작되는지 등의 정보가 있으며, superblock에 crash가 발생하면 filesystem 전체가 마비되는 문제가 발생한다.

xv6는 disk에 값을 쓰고 읽어 오는 단위를 block으로 지정하고 있으며, 이의 IO implementation은 [bio.c](./xv6-public/bio.c)에서 확인할 수 있다.

```c
struct buf {
  int flags;
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  struct buf *qnext; // disk queue
  uchar data[BSIZE];
};
```

xv6는 block을 읽어와 in-memory에서 수정할 수 있도록 buffer cache를 지원하고 있으며, [bio.c](./xv6-public/bio.c)에서는 block을 buffer pool로 읽어와 해당 buffer의 pointer를 반환하는 `bread`, 수정을 마친 buffer를 disk에 곧장 복사하는 `bwrite`의 interface를 구성하고 있다.

xv6의 filesystem 구성은 [fs.c](./xv6-public/fs.c)에서 구현하고 있다. `balloc`과 `bfree`에서 disk block을 할당하거나 해제하고, `struct dinode`를 통해 file abstraction과 disk block 사이의 index를 구성하고 있다. 

```c
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
```

`struct dinode`는 disk에 작성되는 index node의 의미로, in-memory로 읽어와 buffer cache와 같은 역할을 수행하는 구조체는 `struct inode`이다.

[fs.c](./xv6-public/fs.c)에서는 cache에 inode를 할당하는 `ialloc`, disk와 동기화하는 `iupdate`, inode cache에서 inum을 토대로 인덱스를 검색해 오는 `iget`과 공유를 포기하는 `iput`, 삭제의 `itrunc`와 r/w 인터페이스 `readi`, `writei` 등의 메소드로 구성되어 있다. 

사용자가 실제로 다루는 file의 개념은 named index로 볼 수 있다. 이를 DAG의 형태로 계층화한 것이 directory이고, 이는 구분자 '/'를 기준으로 file까지의 link를 관리한다.

[fs.c](./xv6-public/fs.c)에서는 `namei` 메소드를 통해 file path에서 inode의 검색을 지원한다.

```c
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};
```

xv6는 이러한 file을 가리키는 file descriptor, fd를 두고 disk를 향하는 inode와 inter-process communication을 위한 pipe를 최상위 단계로 추상화한다. [file.c](./xv6-public/file.c)에서는 file 구조체를 기반으로 read/write를 수행하는 `fileread`와 `filewrite`를 대표적인 메소드로 가지고 있다.

## 1. Expand maximum file size

현재 xv6의 inode는 addrs 멤버에서 데이터가 작성된 block num을 관리하고 있다. 이때 NDIRECT만큼의 addrs는 데이터가 존재하는 block을 직접 지칭하고 있고, 부가적으로 존재하는 하나의 addr은 indirect block의 block num을 저장하고 있다.

indirect block은 또다시 block num을 가리키고 있으며, [NINDIRECT = BSIZE / sizeof(uint)] 만큼의 추가 데이터 블럭을 구성할 수 있게 해준다.

결과적으로 현재 xv6는 하나의 inode가 [MAXFILE = NDIRECT + NINDIRECT = 140]개의 BLOCK을 소유할 수 있다. 이것이 파일 최대 크기 [MAXFILE * BSIZE = 140 * 512 = 71,680], 대략 71kb 정도로 나타난 것이다.

이를 확장하기 위해서는 다계층의 indirect block을 addr에 저장하고 있어야 한다. 현재는 하나의 indirect block으로 구성된 single indirection으로 구성되어 있다. 이를 하나의 indirect block이 또 다른 indirect block을 가리키는 multiple-indirection으로 구성한다면 하나의 inode는 더 많은 data block을 소유할 수 있다. 

예로 double indirection은 NINDIRECT의 제곱만큼 data block을 소유할 수 있고, triple indirection은 NINDIRECT의 세제곱만큼 data block을 소유할 수 있다. 하지만 조건 없이 multiple-indirect block을 늘려나간다면, 하나의 data block을 찾기 위해 disk의 여러 block에 방문해야 하고, 접근 속도는 더 느려지게 된다.

이에 direct block을 두고, indirect block을 수준별로 하나씩 두면서 크기를 늘려나가는 구조를 선택하면 [MAXFILE = NDIRECT + sum of NINDIRECT ^ k]의 최대 data block 수를 가지게 된다.

구현의 예로 NDIRECT = 12, 최대 triple indirection까지 허용한다면 [MAXFILE = 12 + sum of 128 ^ k = 2,113,676]의 block 수와 최대 1GB 정도의 파일 크기를 가질 수 있다.

### Implementation

### Test

## 2. pread, pwrite

기존의 `read`, `write` system call은 [file.c](./xv6-public/file.c)의 `fileread`와 `filewrite`의 실행을 구성하고 있다.

기본적으로 두 메소드는 읽거나 쓴 byte의 수만큼 stream을 advance하는 side-effect를 가지고 있다. 이는 코드상에서 file 구조체의 offset member에 byte 수를 더하는 방식으로 구현되어 있다.

```c
// in fileread
if((r = readi(f->ip, addr, f->off, n)) > 0)
  f->off += r;

// in filewrite
if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
  f->off += r;
```

기존의 unix system에서 read와 write 메소드를 통해 원하는 곳에 값을 쓰기 위해서는 `lseek`과 같은 메소드를 통해 현재 offset에서 원하는 위치로 이동한 후, 값을 써야 한다. 즉 [lseek - read/write]의 pair로 atomic 한 구성을 띄지 않아 multi-thread 환경에서 하나의 file descriptor에 데이터를 쓰고자 한다면 원하는 곳에 값을 쓰기가 쉽지 않다. 

이를 해결하기 위해서 stream을 advance 하는 side-effect도 제거하고, offset을 인자로 받아 [lseek - read/write]를 하나의 atomic 한 메소드로 구현한 것이 `pread`와 `pwrite`이다.

### Implmentation

[file.c](./xv6-public/file.c)에 두 개의 새로운 메소드 `filepread`와 `filepwrite`를 구성하였다. 

`filepread`는 입력으로 inode의 file descriptor만을 취급하고, `pread`와 동일히 동작하지만 `f->off`의 stream advance를 진행하지 않는다.

```c
// in filepread
r = readi(f->ip, addr, f->off + offset, n);
```

`filepwrite` 또한 마찬가지로 입력으로 inode file descriptor만을 취급한다. `pwrite`가 while-loop 내에서 inode lock을 acquire/release 하는 것에 반해, inode에 주어진 하나의 write operation이 atomicity를 가져야 한다고 생각하여 inode lock의 acquire/release를 while-loop 밖으로 이동하였다.

```c
// in filepwrite
begin_op();
ilock(f->ip);

while (i < n) {
  // ...
}

iunlock(f->ip);
end_op();
```

`filepread`와 마찬가지로 `f->off`의 stream advance 역시 진행하지 않는다.

```c
if ((r = writei(f->ip, addr + i, off, n1)) > 0)
  // do not update f->off
  off += r;
```

이렇게 구현된 `filepread`와 `filepwrite`는 `pread`, `pwrite`의 새로운 syscall을 통해 user가 접근할 수 있다.

### Test

[test_pwrite.c](./xv6-public/test_pwrite.c)에는 pwrite와 pread 메소드를 위한 몇 가지 테스트가 구현되어 있다. 

- test_pwrite1: 파일을 새로 만들고 pwrite의 기본적인 작동 여부를 검사한다. 
- test_pwrite2: 새로운 파일에 write를 진행하여 stream을 advance 시키고, 해당 시점부터 pwrite가 정상 작동하는지 검사한다.
- test_pwrite3: pwrite는 maximum log transaction size의 영향으로, 크기가 큰 write operation은 분할하여 진행한다. 이의 정상작동을 확인하기 위해 page size의 write operation을 진행하여 결과를 확인한다.
- test_pread1: pread의 기본적인 작동 여부를 검사한다.
- test_pread2: read를 진행하여 stream을 advance 시키고, 해당 시점부터 pread가 정상 작동하는지 검사한다.

테스트 결과 모두 정상작동하였다.

```
$ test_pwrite
test_pwrite1 done
test_pwrite2 done
test_pwrite3 done
test_pread1 done
test_pread2 done
```

## 3. Buffer caching
