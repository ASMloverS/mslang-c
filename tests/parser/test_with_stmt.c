// test_with_stmt.c
// T033: unit tests for with statement parsing.
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

static bool psErr(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  msParseStmt(&p);
  return p.hadError;
}

static void testWithAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx as c { }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "with");
  MS_ASSERT_TRUE(n->withStmt.asName != NULL, "has as");
  msArenaFree(&a);
}

static void testWithNoAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx { }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "with");
  MS_ASSERT_TRUE(n->withStmt.asName == NULL, "no as");
  msArenaFree(&a);
}

static void testWithCallExpr(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with open(\"f\") as f { }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "with");
  MS_ASSERT_EQ(n->withStmt.expr->kind, MS_ND_CALL, "call expr");
  MS_ASSERT_TRUE(n->withStmt.asName != NULL, "has as");
  msArenaFree(&a);
}

static void testWithNestedWith(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with a { with b as x { } }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "outer with");
  // body is a block; inside it there should be a with statement
  MS_ASSERT_EQ(n->withStmt.body->kind, MS_ND_BLOCK, "body block");
  msArenaFree(&a);
}

static void testWithAsNonIdent(void) {
  struct MsArena a;
  msArenaInit(&a);
  MS_ASSERT_TRUE(psErr(&a, "with ctx as { }"), "as non-ident error");
  msArenaFree(&a);
}

static void testWithMissingBrace(void) {
  struct MsArena a;
  msArenaInit(&a);
  MS_ASSERT_TRUE(psErr(&a, "with ctx as f pass"), "missing brace error");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testWithAs);
  MS_RUN(testWithNoAs);
  MS_RUN(testWithCallExpr);
  MS_RUN(testWithNestedWith);
  MS_RUN(testWithAsNonIdent);
  MS_RUN(testWithMissingBrace);
  return msTestSummary();
}
