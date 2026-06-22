// test_switch_stmt.c
// T029: unit tests for switch / case / fallthrough / default statement.
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

// switch x { case 1: a } => basic switch
static void testSwitchBasic(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { case 1: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switchStmt.cases != NULL, "has cases");
  msArenaFree(&a);
}

// switch { case x > 0: a } => no switch expression
static void testSwitchNoExpr(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch { case x > 0: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switchStmt.expr == NULL, "no expr");
  msArenaFree(&a);
}

// switch x { case 1, 2: a } => multi-value case
static void testSwitchMultiValue(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { case 1, 2: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MsNode* c = n->switchStmt.cases->node;
  MS_ASSERT_EQ(c->kind, MS_ND_SWITCH_CASE, "case");
  MS_ASSERT_TRUE(c->switchCase.values != NULL, "has val1");
  MS_ASSERT_TRUE(c->switchCase.values->next != NULL, "has val2");
  MS_ASSERT_TRUE(c->switchCase.values->next->next == NULL, "only 2");
  msArenaFree(&a);
}

// switch x { default: a } => default case
static void testSwitchDefault(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { default: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MsNode* c = n->switchStmt.cases->node;
  MS_ASSERT_EQ(c->kind, MS_ND_SWITCH_CASE, "case");
  MS_ASSERT_TRUE(c->switchCase.isDefault, "isDefault");
  MS_ASSERT_TRUE(c->switchCase.values == NULL, "no values");
  msArenaFree(&a);
}

// switch x { case 1: a\nfallthrough\ncase 2: b } => fallthrough in body
static void testSwitchFallthrough(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { case 1: a\nfallthrough\ncase 2: b }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MsNode* c1 = n->switchStmt.cases->node;
  MsNodeList* stmts = c1->switchCase.body->block.stmts;
  MS_ASSERT_TRUE(stmts != NULL, "has stmt1");
  MS_ASSERT_TRUE(stmts->next != NULL, "has stmt2");
  MS_ASSERT_EQ(stmts->next->node->kind, MS_ND_FALLTHROUGH, "fallthrough");
  msArenaFree(&a);
}

// switch x {} => empty switch
static void testSwitchEmpty(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x {}");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switchStmt.cases == NULL, "no cases");
  msArenaFree(&a);
}

// switch x { case 1:\n  a\n  b\ncase 2: c } => multi-line case body
static void testSwitchMultiLineBody(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { case 1:\n  a\n  b\ncase 2: c }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MsNode* c1 = n->switchStmt.cases->node;
  MsNodeList* stmts = c1->switchCase.body->block.stmts;
  MS_ASSERT_TRUE(stmts != NULL, "stmt1");
  MS_ASSERT_TRUE(stmts->next != NULL, "stmt2");
  MS_ASSERT_TRUE(stmts->next->next == NULL, "only 2 stmts");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testSwitchBasic);
  MS_RUN(testSwitchNoExpr);
  MS_RUN(testSwitchMultiValue);
  MS_RUN(testSwitchDefault);
  MS_RUN(testSwitchFallthrough);
  MS_RUN(testSwitchEmpty);
  MS_RUN(testSwitchMultiLineBody);
  return msTestSummary();
}
