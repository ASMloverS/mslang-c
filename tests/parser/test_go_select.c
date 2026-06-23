// test_go_select.c
// T032: unit tests for go / select statement parsing.
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

// go f() => MS_ND_GO(call=MS_ND_CALL)
static void testGoStmt(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "go f()");
  MS_ASSERT_EQ(n->kind, MS_ND_GO, "go");
  MS_ASSERT_EQ(n->goStmt.call->kind, MS_ND_CALL, "call");
  msArenaFree(&a);
}

// select { case <-ch: pass } => MS_ND_SELECT with recv case
static void testSelectRecv(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case <-ch: pass }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MS_ASSERT_TRUE(n->selectStmt.cases != NULL, "has cases");
  MsNode* c = n->selectStmt.cases->node;
  MS_ASSERT_EQ(c->kind, MS_ND_SELECT_CASE, "case kind");
  MS_ASSERT_TRUE(c->selectCase.comm != NULL, "has comm");
  MS_ASSERT_EQ(c->selectCase.comm->kind, MS_ND_RECV, "recv");
  msArenaFree(&a);
}

// select { case v := <-ch: pass } => short decl with recv
static void testSelectRecvAssign(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case v := <-ch: pass }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MsNode* c = n->selectStmt.cases->node;
  MS_ASSERT_EQ(c->selectCase.comm->kind, MS_ND_SHORT_DECL, "short decl");
  MS_ASSERT_TRUE(c->selectCase.comm->varDecl.init != NULL, "has init");
  MS_ASSERT_EQ(c->selectCase.comm->varDecl.init->kind, MS_ND_RECV, "init is recv");
  msArenaFree(&a);
}

// select { case ch <- 1: pass } => send case
static void testSelectSend(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case ch <- 1: pass }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MsNode* c = n->selectStmt.cases->node;
  MS_ASSERT_EQ(c->selectCase.comm->kind, MS_ND_SEND, "send");
  msArenaFree(&a);
}

// select { default: pass } => default case (comm=NULL)
static void testSelectDefault(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { default: pass }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MsNode* c = n->selectStmt.cases->node;
  MS_ASSERT_EQ(c->kind, MS_ND_SELECT_CASE, "case kind");
  MS_ASSERT_TRUE(c->selectCase.comm == NULL, "default no comm");
  msArenaFree(&a);
}

// select {} => empty select (permanent block)
static void testSelectEmpty(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select {}");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MS_ASSERT_TRUE(n->selectStmt.cases == NULL, "no cases");
  msArenaFree(&a);
}

// select { case <-c1: a case <-c2: b default: c } => 3 cases
static void testSelectMultiCase(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case <-c1: a\ncase <-c2: b\ndefault: c }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MS_ASSERT_TRUE(n->selectStmt.cases != NULL, "case1");
  MS_ASSERT_TRUE(n->selectStmt.cases->next != NULL, "case2");
  MS_ASSERT_TRUE(n->selectStmt.cases->next->next != NULL, "case3");
  MS_ASSERT_TRUE(n->selectStmt.cases->next->next->next == NULL, "only 3");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testGoStmt);
  MS_RUN(testSelectRecv);
  MS_RUN(testSelectRecvAssign);
  MS_RUN(testSelectSend);
  MS_RUN(testSelectDefault);
  MS_RUN(testSelectEmpty);
  MS_RUN(testSelectMultiCase);
  return msTestSummary();
}
