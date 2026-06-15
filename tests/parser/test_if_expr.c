// test_if_expr.c
// T020: unit tests for if-expression (ternary) parsing.
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "parser/ms_arena.h"

#include <string.h>

static MsNode* parseStr(struct MsArena* a, const char* src) {
    MsParser p;
    msParserInit(&p, src, (uint32_t)strlen(src), "<t>", a);
    return msParseExpr(&p);
}

// "x if cond else y" => IF_EXPR(thenExpr=IDENT(x), cond=IDENT(cond), elseExpr=IDENT(y))
static void testIfExpr(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "x if cond else y");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,                  MS_ND_IF_EXPR, "root if_expr");
    MS_ASSERT_EQ(n->ifExpr.thenExpr->kind, MS_ND_IDENT,   "thenExpr=ident");
    MS_ASSERT_EQ(n->ifExpr.cond->kind,     MS_ND_IDENT,   "cond=ident");
    MS_ASSERT_EQ(n->ifExpr.elseExpr->kind, MS_ND_IDENT,   "elseExpr=ident");
    msArenaFree(&a);
}

// "1 + 2 if x > 0 else 3" => root IF_EXPR, thenExpr=BINARY(+,1,2)
static void testIfExprBinaryThen(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "1 + 2 if x > 0 else 3");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,                  MS_ND_IF_EXPR, "root if_expr");
    MS_ASSERT_EQ(n->ifExpr.thenExpr->kind, MS_ND_BINARY,  "thenExpr=binary");
    MS_ASSERT_EQ(n->ifExpr.thenExpr->binary.op, MS_TOK_PLUS, "thenExpr op +");
    msArenaFree(&a);
}

// "a if c1 else b if c2 else d" => right-assoc: elseExpr is also IF_EXPR
static void testIfExprChained(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a if c1 else b if c2 else d");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,                   MS_ND_IF_EXPR, "root if_expr");
    MS_ASSERT_EQ(n->ifExpr.elseExpr->kind,  MS_ND_IF_EXPR, "elseExpr is also if_expr (right-assoc)");
    msArenaFree(&a);
}

// Missing 'else' must produce a parse error
static void testIfExprMissingElse(void) {
    struct MsArena a; msArenaInit(&a);
    MsParser p;
    const char* src = "x if cond";
    msParserInit(&p, src, (uint32_t)strlen(src), "<t>", &a);
    msParseExpr(&p);
    MS_ASSERT_TRUE(p.hadError, "hadError set on missing else");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testIfExpr);
    MS_RUN(testIfExprBinaryThen);
    MS_RUN(testIfExprChained);
    MS_RUN(testIfExprMissingElse);
    return msTestSummary();
}
