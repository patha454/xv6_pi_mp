#include "types.h"
#include "stat.h"
#include "user.h"

#define FORK_BENCH 'f'
#define FIB_BENCH 'i'
#define SUM_BENCH 's'

#define FIB_N 30
#define SUM_FILE "benchmark"
#define SUM_BUF_SIZE 1000000

int sum_bench() {
  int i = 1;
  int sum = 0;
  int len;
  int pid;
  char* buf;
  int start = time();
  int fd = open(SUM_FILE, 0);
  buf = malloc(SUM_BUF_SIZE);
  len = read(fd, buf, SUM_BUF_SIZE);
  while (i + 1 * len < SUM_BUF_SIZE) {
    memmove((void*) (buf + i * len), (void*) buf, len);
    i++;
  }
  printf(1, "good copy\n");
  pid = fork();
  if (pid) {
    for (i = 0; i < SUM_BUF_SIZE / 2; i++) {
      sum += buf[i];
    }
    exit();
  } else {
    for (i = SUM_BUF_SIZE / 2; i < SUM_BUF_SIZE; i++) {
      sum += buf[i];
    }
    wait();
  }
  return time() - start;
  
}

int fib(int n) {
  if (n == 1 || n == 2) {
    return 1;
  }
  return fib(n - 1) + fib (n - 2);
}

int fib_bench(int n) {
  int i;
  int pid;
  int start = time();
  for (i = 0; i < n; i++) {
    pid = fork();
    if (!pid) {
      fib(FIB_N);
      exit();
    }
  }
  for (i = 0; i < n; i++) {
    wait();
  }
  int end = time();
  return end - start;
}

int fork_bench(int n) {
  int i;
  int pid;
  int start = time();
  for (i = 0 ; i < n; i++) {
    pid = fork();
    if (!pid) {
      exit();
    }
  }
  for (i = 0; i < n; i++) {
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
    printf(1, "Benchmark sum %d score: %d uS\n", n, score);
    break;
  default:
    break;
  }
  exit();
}
