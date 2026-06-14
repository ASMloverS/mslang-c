// Lexer microbenchmark: iterate all tokens from bench_1k.ms 1000 times.
// Metric: tokens/sec. Target: > 50M tokens/sec in Release build.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mslang/ms_lexer.h"

static char* readFile(const char* path, uint32_t* outLen) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "bench_lexer: cannot open '%s'\n", path);
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  char* buf = malloc((size_t)size + 1);
  size_t n = fread(buf, 1, (size_t)size, fp);
  fclose(fp);
  buf[n] = '\0';
  *outLen = (uint32_t)n;
  return buf;
}

static long long countTokens(const char* src, uint32_t srcLen) {
  struct MsLexer lex;
  msLexerInit(&lex, src, srcLen, "<bench>");
  long long n = 0;
  for (;;) {
    struct MsToken tok = msLexerNext(&lex);
    n++;
    if (tok.kind == MS_TOK_EOF) {
      break;
    }
  }
  return n;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "benchmarks/data/bench_1k.ms";
  uint32_t srcLen = 0;
  char* src = readFile(path, &srcLen);
  if (!src) {
    return 1;
  }

  // Warmup
  long long tokPerIter = countTokens(src, srcLen);

  int numIters = 1000;
  clock_t t0 = clock();
  for (int i = 0; i < numIters; i++) {
    countTokens(src, srcLen);
  }
  clock_t t1 = clock();

  double elapsedSec = (double)(t1 - t0) / CLOCKS_PER_SEC;
  long long totalToks = tokPerIter * numIters;
  double toksPerSec = (double)totalToks / elapsedSec;

  printf("tokens/iter: %lld\n", tokPerIter);
  printf("iters: %d\n", numIters);
  printf("elapsed: %.3f s\n", elapsedSec);
  printf("throughput: %.1f M tokens/sec\n", toksPerSec / 1e6);

  free(src);
  return 0;
}
