// test_make_recv.c
// T025: unit tests for make(chan) and <-ch receive expression parsing.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* px(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  return msParseExpr(&p);
}

static bool pxErr(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  msParseExpr(&p);
  return p.hadError;
}

static void testMakeChanUnbuffered(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "make(chan)");
  MS_ASSERT_EQ(n->kind, MS_ND_MAKE, "make");
  MS_ASSERT_TRUE(n->makeExpr.capExpr == NULL, "unbuffered");
  msArenaFree(&a);
}

static void testMakeChanBuffered(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "make(chan, 16)");
  MS_ASSERT_EQ(n->kind, MS_ND_MAKE, "make");
  MS_ASSERT_TRUE(n->makeExpr.capExpr != NULL, "has cap");
  MS_ASSERT_EQ(n->makeExpr.capExpr->kind, MS_ND_INT, "cap is int");
  MS_ASSERT_EQ(n->makeExpr.capExpr->litInt.ival, 16, "cap=16");
  msArenaFree(&a);
}

static void testRecv(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "<-ch");
  MS_ASSERT_EQ(n->kind, MS_ND_RECV, "recv");
  MS_ASSERT_EQ(n->recv.chanExpr->kind, MS_ND_IDENT, "chan ident");
  msArenaFree(&a);
}

static void testMakeNoChan(void) {
  struct MsArena a;
  msArenaInit(&a);
  MS_ASSERT_TRUE(pxErr(&a, "make()"), "make() error");
  msArenaFree(&a);
}

static void testMakeTrailingComma(void) {
  struct MsArena a;
  msArenaInit(&a);
  MS_ASSERT_TRUE(pxErr(&a, "make(chan,)"), "make(chan,) error");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testMakeChanUnbuffered);
  MS_RUN(testMakeChanBuffered);
  MS_RUN(testRecv);
  MS_RUN(testMakeNoChan);
  MS_RUN(testMakeTrailingComma);
  return msTestSummary();
}
