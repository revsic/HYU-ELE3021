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

## 2. pread, pwrite

## 3. Buffer caching
