// Parser microbenchmark: parse ~300 line .ms program 1000 times.
// Metric: nodes/sec. Target: > 5M nodes/sec in Release build.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static char* readFile(const char* path, uint32_t* outLen) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "bench_parser: cannot open '%s'\n", path);
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  char* buf = malloc((size_t) size + 1);
  size_t n = fread(buf, 1, (size_t) size, fp);
  fclose(fp);
  buf[n] = '\0';
  *outLen = (uint32_t) n;
  return buf;
}

static long long countNodes(MsNode* node) {
  if (!node) {
    return 0;
  }
  long long n = 1;
  switch (node->kind) {
    case MS_ND_PROGRAM:
      for (MsNodeList* c = node->program.stmts; c; c = c->next) {
        n += countNodes(c->node);
      }
      break;
    case MS_ND_BLOCK:
      for (MsNodeList* c = node->block.stmts; c; c = c->next) {
        n += countNodes(c->node);
      }
      break;
    case MS_ND_BINARY:
      n += countNodes(node->binary.left);
      n += countNodes(node->binary.right);
      break;
    case MS_ND_UNARY:
      n += countNodes(node->unary.operand);
      break;
    case MS_ND_EXPR_STMT:
      n += countNodes(node->exprStmt.expr);
      break;
    case MS_ND_FUNC_DECL:
    case MS_ND_ASYNC_FUNC:
      for (MsNodeList* c = node->funcDecl.params; c; c = c->next) {
        n += countNodes(c->node);
      }
      n += countNodes(node->funcDecl.body);
      break;
    case MS_ND_IF:
      n += countNodes(node->ifStmt.cond);
      n += countNodes(node->ifStmt.thenBlock);
      n += countNodes(node->ifStmt.elseBlock);
      break;
    case MS_ND_VAR_DECL:
    case MS_ND_SHORT_DECL:
      n += countNodes(node->varDecl.init);
      break;
    case MS_ND_ASSIGN:
      n += countNodes(node->assign.target);
      n += countNodes(node->assign.value);
      break;
    case MS_ND_COMPOUND_ASSIGN:
      n += countNodes(node->binary.left);
      n += countNodes(node->binary.right);
      break;
    case MS_ND_RETURN:
      n += countNodes(node->singleExpr.expr);
      break;
    case MS_ND_FOR:
      n += countNodes(node->forStmt.init);
      n += countNodes(node->forStmt.cond);
      n += countNodes(node->forStmt.post);
      n += countNodes(node->forStmt.body);
      n += countNodes(node->forStmt.forTarget);
      n += countNodes(node->forStmt.forIter);
      break;
    default:
      break;
  }
  return n;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "benchmarks/data/bench_300l.ms";
  uint32_t srcLen = 0;
  char* src = readFile(path, &srcLen);
  if (!src) {
    return 1;
  }

  // Warmup
  struct MsArena arena;
  msArenaInit(&arena);
  MsParser p;
  msParserInit(&p, src, srcLen, "<bench>", &arena);
  MsNode* prog = msParseProgram(&p);
  long long nodesPerIter = countNodes(prog);
  msArenaFree(&arena);

  int numIters = 1000;
  clock_t t0 = clock();
  for (int i = 0; i < numIters; i++) {
    struct MsArena a;
    msArenaInit(&a);
    MsParser pi;
    msParserInit(&pi, src, srcLen, "<bench>", &a);
    msParseProgram(&pi);
    msArenaFree(&a);
  }
  clock_t t1 = clock();

  double elapsed = (double) (t1 - t0) / CLOCKS_PER_SEC;
  long long totalNodes = nodesPerIter * numIters;
  double nodesPerSec = (double) totalNodes / elapsed;

  printf("nodes/iter: %lld\n", nodesPerIter);
  printf("iters: %d\n", numIters);
  printf("elapsed: %.3f s\n", elapsed);
  printf("throughput: %.1f M nodes/sec\n", nodesPerSec / 1e6);

  free(src);
  return 0;
}
