// test_for_stmt.c
// T028: unit tests for for statement (infinite, condition, three-part, for-in).
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* pStmt(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  return msParseStmt(&p);
}

// for { } => infinite loop
static void testForInfinite(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.cond == NULL, "no cond");
  MS_ASSERT_TRUE(n->forStmt.init == NULL, "no init");
  MS_ASSERT_TRUE(n->forStmt.post == NULL, "no post");
  MS_ASSERT_TRUE(n->forStmt.forTarget == NULL, "no target");
  MS_ASSERT_TRUE(n->forStmt.forIter == NULL, "no iter");
  msArenaFree(&a);
}

// for i in lst { } => for-in single variable
static void testForIn(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i in lst { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.forTarget != NULL, "has target");
  MS_ASSERT_TRUE(n->forStmt.forIter != NULL, "has iter");
  MS_ASSERT_EQ(n->forStmt.forTarget->kind, MS_ND_IDENT, "target=ident");
  MS_ASSERT_EQ(n->forStmt.forIter->kind, MS_ND_IDENT, "iter=ident");
  msArenaFree(&a);
}

// for k, v in d { } => for-in with tuple target
static void testForInTuple(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for k, v in d { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.forTarget != NULL, "has target");
  MS_ASSERT_EQ(n->forStmt.forTarget->kind, MS_ND_TUPLE, "target=tuple");
  MS_ASSERT_TRUE(n->forStmt.forIter != NULL, "has iter");
  MsNodeList* elems = n->forStmt.forTarget->container.elems;
  MS_ASSERT_TRUE(elems != NULL, "elem1");
  MS_ASSERT_TRUE(elems->next != NULL, "elem2");
  MS_ASSERT_TRUE(elems->next->next == NULL, "only 2");
  msArenaFree(&a);
}

// for x > 0 { } => condition loop
static void testForCond(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for x > 0 { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.cond != NULL, "has cond");
  MS_ASSERT_EQ(n->forStmt.cond->kind, MS_ND_BINARY, "cond is binary");
  MS_ASSERT_TRUE(n->forStmt.forTarget == NULL, "no target");
  msArenaFree(&a);
}

// for i := 0; i < 10; i++ { } => three-part
static void testForThreePart(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i := 0; i < 10; i++ { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.init != NULL, "has init");
  MS_ASSERT_TRUE(n->forStmt.cond != NULL, "has cond");
  MS_ASSERT_TRUE(n->forStmt.post != NULL, "has post");
  MS_ASSERT_TRUE(n->forStmt.forTarget == NULL, "no target");
  MS_ASSERT_EQ(n->forStmt.init->kind, MS_ND_SHORT_DECL, "init=short_decl");
  MS_ASSERT_EQ(n->forStmt.cond->kind, MS_ND_BINARY, "cond=binary");
  MS_ASSERT_EQ(n->forStmt.post->kind, MS_ND_INC_DEC, "post=inc_dec");
  msArenaFree(&a);
}

// for x in [1, 2, 3] { } => for-in with list iter
static void testForInList(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for x in [1, 2, 3] { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.forTarget != NULL, "has target");
  MS_ASSERT_EQ(n->forStmt.forTarget->kind, MS_ND_IDENT, "target=ident");
  MS_ASSERT_TRUE(n->forStmt.forIter != NULL, "has iter");
  MS_ASSERT_EQ(n->forStmt.forIter->kind, MS_ND_LIST, "iter=list");
  msArenaFree(&a);
}

// for i in range(10) { } => for-in with call iter
static void testForInCall(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i in range(10) { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_EQ(n->forStmt.forTarget->kind, MS_ND_IDENT, "target=ident");
  MS_ASSERT_TRUE(n->forStmt.forIter != NULL, "has iter");
  MS_ASSERT_EQ(n->forStmt.forIter->kind, MS_ND_CALL, "iter=call");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testForInfinite);
  MS_RUN(testForIn);
  MS_RUN(testForInTuple);
  MS_RUN(testForCond);
  MS_RUN(testForThreePart);
  MS_RUN(testForInList);
  MS_RUN(testForInCall);
  return msTestSummary();
}
