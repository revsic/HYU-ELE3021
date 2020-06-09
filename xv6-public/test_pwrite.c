#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define ASSERT_(x, n, line, sub) if ((x) != n) { printf(1, "wrong in line %d %d\n", line, sub); exit(); }

#define ASSERT(x, n) ASSERT_(x, n, __LINE__, 0)

int strncmp(const char* a, const char* b, int len) {
  int i;
  for (i = 0; i < len; ++i)
    if (a[i] != b[i])
      return a[i] - b[i];
  return 0;
}

void readtest(const char* filename, const char* answer, int line) {
  char buffer[1024];
  int len = strlen(answer);
  int fd = open(filename, O_RDONLY);
  ASSERT_(read(fd, buffer, len + 1), len + 1, line, 0);
  ASSERT_(strlen(buffer), len, line, 1);
  ASSERT_(strcmp(buffer, answer), 0, line, 2);
  close(fd);
}

// case 1. pwrite on begining
void test_pwrite1() {
  int fd = open("testfile", O_CREATE|O_WRONLY);
  // result: asdf
  ASSERT(pwrite(fd, "asdf", 5, 0), 5);
  readtest("testfile", "asdf", __LINE__);

  // result: asqwert
  ASSERT(pwrite(fd, "qwert", 6, 2), 6);
  readtest("testfile", "asqwert", __LINE__);

  // result: aoperrt
  ASSERT(pwrite(fd, "oper", 4, 1), 4);
  // result: zxperrt
  ASSERT(pwrite(fd, "zx", 2, 0), 2);
  readtest("testfile", "zxperrt", __LINE__);

  printf(1, "test_pwrite1 done\n");
  close(fd);
}

// case 2. pwrite after write
void test_pwrite2() {
  int fd = open("testfile", O_CREATE|O_WRONLY);
  // result: asdf
  ASSERT(write(fd, "asdf", 4), 4);
  // result: asdfqwert
  ASSERT(pwrite(fd, "qwert", 6, 0), 6);
  // result: asdfqzxrt
  ASSERT(pwrite(fd, "zx", 2, 1), 2);
  readtest("testfile", "asdfqzxrt", __LINE__);

  printf(1, "test_pwrite2 done\n");
  close(fd);
}

int __test_pwrite3_buffer[1024];
// case 3. pwrite more than 1536 bytes (maximum log transaction size)
void test_pwrite3() {
  int i;
  int *buffer = __test_pwrite3_buffer;
  for (i = 0; i < 1024; ++i)
    buffer[i] = i;

  int fd = open("testfile", O_CREATE|O_WRONLY);
  ASSERT(pwrite(fd, buffer, sizeof(buffer), 0), sizeof(buffer));
  close(fd);

  memset(buffer, 0, sizeof(buffer));

  fd = open("testfile", O_RDONLY);
  read(fd, buffer, sizeof(buffer));

  for (i = 0; i < 1024; ++i)
    if (buffer[i] != i) {
      printf(1, "wrong in line %d, value i=%d\n", __LINE__, i);
      exit();
    }
  
  printf(1, "test_pwrite3 done\n");
  close(fd);
}

// case1. pread on beginning.
void test_pread1() {
  int fd = open("testfile", O_CREATE|O_RDWR);
  ASSERT(pwrite(fd, "asdfqwerzxcv", 13, 0), 13);

  char buffer[1024];
  ASSERT(pread(fd, buffer, 4, 0), 4);
  ASSERT(strncmp(buffer, "asdf", 4), 0);
  ASSERT(pread(fd, buffer, 5, 3), 5);
  ASSERT(strncmp(buffer, "fqwer", 5), 0);
  ASSERT(pread(fd, buffer, 5, 8), 5);
  ASSERT(strcmp(buffer, "zxcv"), 0);

  printf(1, "test_pread1 done\n");
  close(fd);
}

// case2. pread after read.
void test_pread2() {
  int fd = open("testfile", O_CREATE|O_RDWR);
  ASSERT(pwrite(fd, "asdfqwerzxcv", 13, 0), 13);

  char buffer[1024];
  ASSERT(read(fd, buffer, 4), 4);
  ASSERT(pread(fd, buffer, 4, 0), 4);
  ASSERT(strncmp(buffer, "qwer", 4), 0);
  ASSERT(pread(fd, buffer, 5, 3), 5);
  ASSERT(strncmp(buffer, "rzxcv", 5), 0);

  ASSERT(pwrite(fd, "1234567", 7, 0), 7);
  ASSERT(pread(fd, buffer, 9, 0), 9);
  ASSERT(strcmp(buffer, "1234567v"), 0);

  printf(1, "test_pread2 done\n");
  close(fd);
}

int main(int argc, char *argv[]) {
  test_pwrite1();
  test_pwrite2();
  test_pwrite3();
  test_pread1();
  test_pread2();
  exit();
  return 0;
}