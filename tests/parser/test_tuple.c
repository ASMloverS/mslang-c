// test_tuple.c
// T023: unit tests for grouping-parenthesis / tuple literal parsing.
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

static int listLen(MsNodeList* l) {
  int n = 0;
  while (l) {
    n++;
    l = l->next;
  }
  return n;
}

static void testSingleElemTuple(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "(42,)");
  MS_ASSERT_EQ(n->kind, MS_ND_TUPLE, "single-elem tuple");
  MS_ASSERT_TRUE(n->container.elems != NULL, "has one element");
  MS_ASSERT_TRUE(n->container.elems->next == NULL, "only one");
  msArenaFree(&a);
}

static void testGrouping(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "(1 + 2)");
  MS_ASSERT_EQ(n->kind, MS_ND_BINARY, "grouping: returns binary, not tuple");
  msArenaFree(&a);
}

static void testTuple3(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "(1, 2, 3)");
  MS_ASSERT_EQ(n->kind, MS_ND_TUPLE, "tuple 3");
  MS_ASSERT_EQ(listLen(n->container.elems), 3, "3 elements");
  msArenaFree(&a);
}

static void testGroupingIdent(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "(x)");
  MS_ASSERT_EQ(n->kind, MS_ND_IDENT, "grouping ident: no tuple wrapper");
  msArenaFree(&a);
}

static void testTupleTrailingComma(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "(1, 2,)");
  MS_ASSERT_EQ(n->kind, MS_ND_TUPLE, "trailing comma tuple kind");
  MS_ASSERT_EQ(listLen(n->container.elems), 2, "2 elements");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testSingleElemTuple);
  MS_RUN(testGrouping);
  MS_RUN(testTuple3);
  MS_RUN(testGroupingIdent);
  MS_RUN(testTupleTrailingComma);
  return msTestSummary();
}
