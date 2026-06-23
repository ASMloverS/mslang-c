// test_jump_stmts.c
// T030: unit tests for return / break / continue / pass / del / assert.
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

// return (no value)
static void testReturnEmpty(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "return");
  MS_ASSERT_EQ(n->kind, MS_ND_RETURN, "return");
  MS_ASSERT_TRUE(n->singleExpr.expr == NULL, "no expr");
  msArenaFree(&a);
}

// return 42
static void testReturnValue(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "return 42");
  MS_ASSERT_EQ(n->kind, MS_ND_RETURN, "return");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has expr");
  MS_ASSERT_EQ(n->singleExpr.expr->kind, MS_ND_INT, "int");
  msArenaFree(&a);
}

// break
static void testBreak(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "break");
  MS_ASSERT_EQ(n->kind, MS_ND_BREAK, "break");
  MS_ASSERT_TRUE(n->jump.label == NULL, "no label");
  msArenaFree(&a);
}

// continue
static void testContinue(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "continue");
  MS_ASSERT_EQ(n->kind, MS_ND_CONTINUE, "continue");
  MS_ASSERT_TRUE(n->jump.label == NULL, "no label");
  msArenaFree(&a);
}

// pass
static void testPass(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "pass");
  MS_ASSERT_EQ(n->kind, MS_ND_PASS, "pass");
  msArenaFree(&a);
}

// del x
static void testDelIdent(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "del x");
  MS_ASSERT_EQ(n->kind, MS_ND_DEL, "del");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has target");
  MS_ASSERT_EQ(n->singleExpr.expr->kind, MS_ND_IDENT, "ident");
  msArenaFree(&a);
}

// del a[0]
static void testDelIndex(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "del a[0]");
  MS_ASSERT_EQ(n->kind, MS_ND_DEL, "del");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has target");
  MS_ASSERT_EQ(n->singleExpr.expr->kind, MS_ND_INDEX, "index");
  msArenaFree(&a);
}

// del obj.x
static void testDelAttr(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "del obj.x");
  MS_ASSERT_EQ(n->kind, MS_ND_DEL, "del");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has target");
  MS_ASSERT_EQ(n->singleExpr.expr->kind, MS_ND_ATTR, "attr");
  msArenaFree(&a);
}

// assert cond
static void testAssertNoMsg(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "assert cond");
  MS_ASSERT_EQ(n->kind, MS_ND_ASSERT, "assert");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has cond");
  MS_ASSERT_TRUE(n->singleExpr.expr2 == NULL, "no msg");
  msArenaFree(&a);
}

// assert x > 0, "must be positive"
static void testAssertWithMsg(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "assert x > 0, \"must be positive\"");
  MS_ASSERT_EQ(n->kind, MS_ND_ASSERT, "assert");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has cond");
  MS_ASSERT_TRUE(n->singleExpr.expr2 != NULL, "has msg");
  MS_ASSERT_EQ(n->singleExpr.expr2->kind, MS_ND_STRING, "msg str");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testReturnEmpty);
  MS_RUN(testReturnValue);
  MS_RUN(testBreak);
  MS_RUN(testContinue);
  MS_RUN(testPass);
  MS_RUN(testDelIdent);
  MS_RUN(testDelIndex);
  MS_RUN(testDelAttr);
  MS_RUN(testAssertNoMsg);
  MS_RUN(testAssertWithMsg);
  return msTestSummary();
}
