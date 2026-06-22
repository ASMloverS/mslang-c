// test_if_stmt.c
// T027: unit tests for if / else if / else statement and parseBlock.
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

static MsNode* pStmtErr(struct MsArena* a, const char* s, MsParser* out) {
  msParserInit(out, s, (uint32_t) strlen(s), "<t>", a);
  return msParseStmt(out);
}

// if x { } => MS_ND_IF, no else
static void testIfSimple(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if stmt");
  MS_ASSERT_EQ(n->ifStmt.cond->kind, MS_ND_IDENT, "cond ident");
  MS_ASSERT_EQ(n->ifStmt.thenBlock->kind, MS_ND_BLOCK, "then block");
  MS_ASSERT_TRUE(n->ifStmt.thenBlock->block.stmts == NULL, "empty block");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock == NULL, "no else");
  msArenaFree(&a);
}

// if x { a } else { b }
static void testIfElse(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { a } else { b }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock != NULL, "has else");
  MS_ASSERT_EQ(n->ifStmt.elseBlock->kind, MS_ND_BLOCK, "else is block");
  // else block has one stmt
  MS_ASSERT_TRUE(n->ifStmt.elseBlock->block.stmts != NULL, "else has stmts");
  MsNode* elseStmt = n->ifStmt.elseBlock->block.stmts->node;
  MS_ASSERT_EQ(elseStmt->kind, MS_ND_EXPR_STMT, "else stmt");
  msArenaFree(&a);
}

// if x { a } else if y { b } else { c }
static void testIfElseIf(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { a } else if y { b } else { c }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "outer if");
  MS_ASSERT_EQ(n->ifStmt.elseBlock->kind, MS_ND_IF, "else if");
  MsNode* inner = n->ifStmt.elseBlock;
  MS_ASSERT_EQ(inner->ifStmt.cond->kind, MS_ND_IDENT, "inner cond");
  MS_ASSERT_TRUE(inner->ifStmt.elseBlock != NULL, "inner has else");
  MS_ASSERT_EQ(inner->ifStmt.elseBlock->kind, MS_ND_BLOCK, "final else block");
  msArenaFree(&a);
}

// if x > 0 and y < 10 { } => cond is binary AND
static void testIfComplexCond(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x > 0 and y < 10 { }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if stmt");
  MS_ASSERT_EQ(n->ifStmt.cond->kind, MS_ND_BINARY, "cond binary");
  MS_ASSERT_EQ(n->ifStmt.cond->binary.op, MS_TOK_AND, "cond and");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock == NULL, "no else");
  msArenaFree(&a);
}

// multiline block: "if x {\n  a\n} else {\n  b\n}"
static void testIfElseMultiline(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x {\n  a\n} else {\n  b\n}");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "multiline if");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock != NULL, "has else");
  MS_ASSERT_EQ(n->ifStmt.elseBlock->kind, MS_ND_BLOCK, "else block");
  msArenaFree(&a);
}

// "if x {\n}\nelse { }" => newline after } breaks else attachment
static void testIfElseNewlineError(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsParser p;
  MsNode* n = pStmtErr(&a, "if x {\n}\nelse { }", &p);
  // } then newline triggers ASI, so "else" is a separate (invalid) statement.
  // The if itself parses fine but has no else branch.
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if parsed");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock == NULL, "no else attached");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testIfSimple);
  MS_RUN(testIfElse);
  MS_RUN(testIfElseIf);
  MS_RUN(testIfComplexCond);
  MS_RUN(testIfElseMultiline);
  MS_RUN(testIfElseNewlineError);
  return msTestSummary();
}
