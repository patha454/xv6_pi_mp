#include "types.h"
#include "stat.h"
#include "user.h"

#define FORK_BENCH 'f'

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
    printf(1, "Benchmark Fork %d score: %duS\n", n, score);
    break;
  default:
    break;
  }
  exit();
}
