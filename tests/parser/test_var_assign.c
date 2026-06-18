// test_var_assign.c
// T026: unit tests for var declaration, short declaration, and assignment
// statement parsing.
#include <string.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* pStmt(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  return msParseStmt(&p);
}

static void testVarDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "var x = 42");
  MS_ASSERT_EQ(n->kind, MS_ND_VAR_DECL, "var decl");
  MS_ASSERT_EQ(n->varDecl.nameLen, 1, "name len");
  MS_ASSERT_TRUE(n->varDecl.init != NULL, "has init");
  MS_ASSERT_EQ(n->varDecl.init->kind, MS_ND_INT, "init kind");
  MS_ASSERT_EQ(n->varDecl.init->litInt.ival, 42, "init=42");
  msArenaFree(&a);
}

static void testVarDeclNoInit(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "var x");
  MS_ASSERT_EQ(n->kind, MS_ND_VAR_DECL, "var decl no init");
  MS_ASSERT_EQ(n->varDecl.nameLen, 1, "name len");
  MS_ASSERT_TRUE(n->varDecl.init == NULL, "no init");
  msArenaFree(&a);
}

static void testShortDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "x := 42");
  MS_ASSERT_EQ(n->kind, MS_ND_SHORT_DECL, "short decl");
  MS_ASSERT_EQ(n->varDecl.nameLen, 1, "name len");
  MS_ASSERT_TRUE(n->varDecl.init != NULL, "has init");
  MS_ASSERT_EQ(n->varDecl.init->kind, MS_ND_INT, "init kind");
  MS_ASSERT_EQ(n->varDecl.init->litInt.ival, 42, "init=42");
  msArenaFree(&a);
}

static void testPlainAssign(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "x = 10");
  MS_ASSERT_EQ(n->kind, MS_ND_ASSIGN, "plain assign");
  MS_ASSERT_EQ(n->assign.target->kind, MS_ND_IDENT, "target ident");
  MS_ASSERT_EQ(n->assign.value->kind, MS_ND_INT, "value int");
  MS_ASSERT_EQ(n->assign.value->litInt.ival, 10, "value=10");
  msArenaFree(&a);
}

static void testCompoundAssign(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "x += 5");
  MS_ASSERT_EQ(n->kind, MS_ND_COMPOUND_ASSIGN, "compound assign");
  MS_ASSERT_EQ(n->binary.op, MS_TOK_PLUS_ASSIGN, "+=");
  MS_ASSERT_EQ(n->binary.left->kind, MS_ND_IDENT, "left ident");
  MS_ASSERT_EQ(n->binary.right->kind, MS_ND_INT, "right int");
  MS_ASSERT_EQ(n->binary.right->litInt.ival, 5, "right=5");
  msArenaFree(&a);
}

static void testAttrAssign(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "a.b = 1");
  MS_ASSERT_EQ(n->kind, MS_ND_ASSIGN, "attr assign");
  MS_ASSERT_EQ(n->assign.target->kind, MS_ND_ATTR, "target attr");
  MS_ASSERT_EQ(n->assign.value->kind, MS_ND_INT, "value int");
  MS_ASSERT_EQ(n->assign.value->litInt.ival, 1, "value=1");
  msArenaFree(&a);
}

static void testIndexAssign(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "a[0] = 2");
  MS_ASSERT_EQ(n->kind, MS_ND_ASSIGN, "index assign");
  MS_ASSERT_EQ(n->assign.target->kind, MS_ND_INDEX, "target index");
  MS_ASSERT_EQ(n->assign.value->kind, MS_ND_INT, "value int");
  MS_ASSERT_EQ(n->assign.value->litInt.ival, 2, "value=2");
  msArenaFree(&a);
}

static void testAllCompoundOps(void) {
  struct MsArena a;

  struct {
    const char* src;
    MsTokKind op;
    const char* label;
  } cases[] = {
      {"x += 1", MS_TOK_PLUS_ASSIGN, "+="},
      {"x -= 1", MS_TOK_MINUS_ASSIGN, "-="},
      {"x *= 1", MS_TOK_STAR_ASSIGN, "*="},
      {"x /= 1", MS_TOK_SLASH_ASSIGN, "/="},
      {"x %= 1", MS_TOK_PERCENT_ASSIGN, "%="},
      {"x &= 1", MS_TOK_AMP_ASSIGN, "&="},
      {"x |= 1", MS_TOK_PIPE_ASSIGN, "|="},
      {"x ^= 1", MS_TOK_CARET_ASSIGN, "^="},
      {"x <<= 1", MS_TOK_SHL_ASSIGN, "<<="},
      {"x >>= 1", MS_TOK_SHR_ASSIGN, ">>="},
  };

  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    msArenaInit(&a);
    MsNode* n = pStmt(&a, cases[i].src);
    MS_ASSERT_EQ(n->kind, MS_ND_COMPOUND_ASSIGN, cases[i].label);
    MS_ASSERT_EQ(n->binary.op, cases[i].op, cases[i].label);
    msArenaFree(&a);
  }
}

int main(void) {
  MS_RUN(testVarDecl);
  MS_RUN(testVarDeclNoInit);
  MS_RUN(testShortDecl);
  MS_RUN(testPlainAssign);
  MS_RUN(testCompoundAssign);
  MS_RUN(testAttrAssign);
  MS_RUN(testIndexAssign);
  MS_RUN(testAllCompoundOps);
  return msTestSummary();
}
