#include "types.h"
#include "stat.h"
#include "user.h"

#define FORK_BENCH 'f'
#define FIB_BENCH 'i'
#define SUM_BENCH 's'

#define FIB_N 20
#define SUM_FILE "benchmark"
#define SUM_BUF_SIZE 10000

void sum() {
  int i = 1;
  int sum = 0;
  int len;
  int pid;
  char* buf;
  int fd;
  while ((fd = open(SUM_FILE, 0)) < 0) {
  }
  buf = malloc(SUM_BUF_SIZE);
  if (buf == 0) {
    printf(1, "Malloc failure\n");
    exit();
  }
  len = read(fd, buf, SUM_BUF_SIZE);
  while ((i + 1) * len < SUM_BUF_SIZE) {
    memmove((void*) (buf + i * len), (void*) buf, len);
    i++;
  }
  while ((pid = fork()) == -1) {   
  }
  if (!pid) {
    for (i = 0; i < SUM_BUF_SIZE / 2; i++) {
      sum += buf[i];
    }
    exit();
  } else {
    for (i = SUM_BUF_SIZE / 2; i < SUM_BUF_SIZE; i++) {
      sum += buf[i];
    }
  }
  free(buf);
  close(fd);
  wait();
  return;
}

int sum_bench(int n) {
  int start = time();
  int i;
  int pid;
  for (i = 0; i < n; i++) {
    while ((pid = fork()) == -1) {
      wait();
    }
    if (!pid) {
      sum();
      exit();
    }
  }
  for (i = 0; i < n; i++) {
    wait();
  } 
  return time() - start;
}

int fib(int n) {
  if (n <= 2) {
    return 1;
  }
  return fib(n - 1) + fib (n - 2);
}

int fib_bench(int n) {
  int i;
  int j;
  int pid;
  int start = time();
  j = 0;
  for (i = 0; i < n; i++) {
    while ((pid = fork()) == -1) {
      wait();
      j--;
    }
    j++;
    if (!pid) {
      fib(FIB_N);
      exit();
    }
  }
  for (i = 0; i < j; i++) {
    wait();
  }
  int end = time();
  return end - start;
}

int fork_bench(int n) {
  int i;
  int j;
  int pid;
  int start = time();
  j = 0;
  for (i = 0 ; i < n; i++) {
    while ((pid = fork()) == -1) {
      wait();
      j--;
    }
    j++;
    if (!pid) {
      exit();
    }
  }
  for (i = 0; i < j; i++) {
    wait();
  }
  int end = time();
  int time = end - start;
  return time;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf(1, "Usage: \"benchmark [test] [n]\"");
    exit();
  }
  char benchmark = argv[1][0];
  int n = atoi(argv[2]);
  int score;
  switch (benchmark) {
  case FORK_BENCH:
    score = fork_bench(n);
    printf(1, "Benchmark Fork %d score: %d uS\n", n, score);
    break;
  case FIB_BENCH:
    score = fib_bench(n);
    printf(1, "Benchmark Fib %d score: %d uS\n", n, score);
    break;
  case SUM_BENCH:
    score = sum_bench(n);
    printf(1, "Benchmark Sum %d score: %d uS\n", n, score);
    break;
  default:
    break;
  }
  exit();
}
