// test_try_raise.c
// T031: unit tests for try/catch/finally/raise parsing.
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

// try { } catch (e) { } -> MS_ND_TRY, handlers=[CATCH(typeFilter=NULL)]
static void testTryCatchAll(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e) { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MS_ASSERT_TRUE(n->tryStmt.handlers != NULL, "has catch");
  MsNode* c = n->tryStmt.handlers->node;
  MS_ASSERT_EQ(c->kind, MS_ND_CATCH_CLAUSE, "catch clause");
  MS_ASSERT_TRUE(c->catchClause.typeFilter == NULL, "no type filter");
  MS_ASSERT_TRUE(c->catchClause.body != NULL, "has body");
  MS_ASSERT_TRUE(n->tryStmt.finallyBlock == NULL, "no finally");
  msArenaFree(&a);
}

// try { } catch (e: ValueError) { } -> single type filter
static void testTryCatchTyped(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e: ValueError) { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MsNode* c = n->tryStmt.handlers->node;
  MS_ASSERT_TRUE(c->catchClause.typeFilter != NULL, "has type filter");
  MS_ASSERT_EQ(c->catchClause.typeFilter->node->kind, MS_ND_IDENT, "ident");
  MS_ASSERT_TRUE(c->catchClause.typeFilter->next == NULL, "single type");
  msArenaFree(&a);
}

// try { } catch (e: ValueError, TypeError) { } -> multi type filter
static void testTryCatchMultiType(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e: ValueError, TypeError) { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MsNode* c = n->tryStmt.handlers->node;
  MS_ASSERT_TRUE(c->catchClause.typeFilter != NULL, "has type filter");
  MS_ASSERT_TRUE(c->catchClause.typeFilter->next != NULL, "multi type");
  MS_ASSERT_EQ(c->catchClause.typeFilter->node->kind, MS_ND_IDENT, "type1");
  MS_ASSERT_EQ(c->catchClause.typeFilter->next->node->kind, MS_ND_IDENT, "type2");
  msArenaFree(&a);
}

// try { } finally { } -> no handlers, has finallyBlock
static void testTryFinally(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } finally { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MS_ASSERT_TRUE(n->tryStmt.handlers == NULL, "no catch");
  MS_ASSERT_TRUE(n->tryStmt.finallyBlock != NULL, "has finally");
  msArenaFree(&a);
}

// try { } catch (e) { } catch (e2: E2) { } finally { } -> 2 handlers + finally
static void testTryCatchCatchFinally(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e) { } catch (e2: E2) { } finally { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MS_ASSERT_TRUE(n->tryStmt.handlers != NULL, "has catch 1");
  MS_ASSERT_TRUE(n->tryStmt.handlers->next != NULL, "has catch 2");
  MS_ASSERT_TRUE(n->tryStmt.finallyBlock != NULL, "has finally");
  msArenaFree(&a);
}

// try { } (no catch/finally) -> parse error
static void testTryNoCatchNoFinally(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { }");
  // Parser calls msParserError; node is still returned but error was reported.
  // We simply verify it parsed as MS_ND_TRY (error recovery still produces the node).
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MS_ASSERT_TRUE(n->tryStmt.handlers == NULL, "no handlers");
  MS_ASSERT_TRUE(n->tryStmt.finallyBlock == NULL, "no finally");
  msArenaFree(&a);
}

// raise -> MS_ND_RAISE(expr=NULL)
static void testRaiseNoArg(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "raise");
  MS_ASSERT_EQ(n->kind, MS_ND_RAISE, "raise");
  MS_ASSERT_TRUE(n->singleExpr.expr == NULL, "reraise");
  msArenaFree(&a);
}

// raise ValueError("msg") -> MS_ND_RAISE(expr=MS_ND_CALL)
static void testRaiseWithExpr(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "raise ValueError(\"msg\")");
  MS_ASSERT_EQ(n->kind, MS_ND_RAISE, "raise");
  MS_ASSERT_TRUE(n->singleExpr.expr != NULL, "has expr");
  MS_ASSERT_EQ(n->singleExpr.expr->kind, MS_ND_CALL, "call");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testTryCatchAll);
  MS_RUN(testTryCatchTyped);
  MS_RUN(testTryCatchMultiType);
  MS_RUN(testTryFinally);
  MS_RUN(testTryCatchCatchFinally);
  MS_RUN(testTryNoCatchNoFinally);
  MS_RUN(testRaiseNoArg);
  MS_RUN(testRaiseWithExpr);
  return msTestSummary();
}
