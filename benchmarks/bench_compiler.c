// Compiler throughput microbenchmark: compile a 500K-line source string
// (source -> MsChunk) 5 times. Metric: lines/sec. Target: >= 0.5M lines/sec.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mslang/ms_compiler.h"

// Fill buf with `lines` copies of a simple statement, one per line.
// Returns the number of lines actually written (*outLen bytes, excluding
// the NUL) — may be less than `lines` if cap is too small.
static uint32_t genBenchSrc(char* buf, uint32_t cap, uint32_t lines, uint32_t* outLen) {
  static const char kLine[] = "x := 1 + 2\n";
  size_t lineLen = sizeof(kLine) - 1;
  uint32_t len = 0;
  uint32_t i = 0;
  for (; i < lines && len + lineLen < cap; i++) {
    memcpy(buf + len, kLine, lineLen);
    len += (uint32_t) lineLen;
  }
  buf[len] = '\0';
  *outLen = len;
  return i;
}

int main(void) {
  uint32_t cap = 32 * 1024 * 1024;
  char* src = malloc(cap);
  if (!src) {
    fprintf(stderr, "bench_compiler: out of memory\n");
    return 1;
  }
  uint32_t len = 0;
  uint32_t lines = genBenchSrc(src, cap, 500000, &len);

  int runs = 5;
  double totalSec = 0;
  for (int i = 0; i < runs; i++) {
    clock_t t0 = clock();
    MsCompileResult result = msCompile(src, len, "<bench>");
    clock_t t1 = clock();
    totalSec += (double) (t1 - t0) / CLOCKS_PER_SEC;
    msCompileResultFree(&result);
  }

  double avgSec = totalSec / runs;
  double linesPerSec = (double) lines / avgSec;

  printf("lines: %u\n", lines);
  printf("runs: %d\n", runs);
  printf("avg elapsed: %.3f s\n", avgSec);
  printf("throughput: %.2f M lines/sec\n", linesPerSec / 1e6);

  free(src);
  return 0;
}
