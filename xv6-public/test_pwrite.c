#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define ASSERT_(x, n, line, sub) if ((x) != n) { printf(1, "wrong in line %d %d\n", line, sub); exit(); }

#define ASSERT(x, n) ASSERT_(x, n, __LINE__, 0)

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

int main(int argc, char *argv[]) {
  test_pwrite1();
  test_pwrite2();
  test_pwrite3();
  exit();
  return 0;
}