// test_call_attr.c
// T021: unit tests for call/attr/index/slice/postfix parsing.
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "parser/ms_arena.h"

#include <string.h>

static MsNode* px(struct MsArena* a, const char* s) {
    MsParser p;
    msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
    return msParseExpr(&p);
}

static void testEmptyCall(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f()");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.args == NULL, "no args");
    MS_ASSERT_TRUE(n->call.kwargs == NULL, "no kwargs");
    msArenaFree(&a);
}

static void testPositionalArgs(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(1, 2)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.args != NULL, "has args");
    MS_ASSERT_EQ(n->call.args->node->kind, MS_ND_INT, "first arg int");
    MS_ASSERT_EQ(n->call.args->next->node->kind, MS_ND_INT, "second arg int");
    msArenaFree(&a);
}

static void testKwarg(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(x=1)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.kwargs != NULL, "has kwargs");
    MS_ASSERT_EQ(n->call.kwargs->node->kind, MS_ND_KWARG_PAIR, "kwarg");
    msArenaFree(&a);
}

static void testStarArgs(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(*a, **b)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.args != NULL, "has star arg");
    MS_ASSERT_EQ(n->call.args->node->kind, MS_ND_STAR_EXPR, "star_expr");
    MS_ASSERT_TRUE(n->call.kwargs != NULL, "has doublestar arg");
    MS_ASSERT_EQ(n->call.kwargs->node->kind, MS_ND_DOUBLESTAR_EXPR, "doublestar_expr");
    msArenaFree(&a);
}

static void testTrailingComma(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(1,)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.args != NULL, "has arg");
    msArenaFree(&a);
}

static void testAttr(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "obj.x");
    MS_ASSERT_EQ(n->kind, MS_ND_ATTR, "attr");
    MS_ASSERT_EQ(n->attr.obj->kind, MS_ND_IDENT, "obj is ident");
    msArenaFree(&a);
}

static void testAttrChain(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a.b.c");
    MS_ASSERT_EQ(n->kind,           MS_ND_ATTR, "outer attr c");
    MS_ASSERT_EQ(n->attr.obj->kind, MS_ND_ATTR, "inner attr b");
    msArenaFree(&a);
}

static void testIndex(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a[0]");
    MS_ASSERT_EQ(n->kind, MS_ND_INDEX, "index");
    MS_ASSERT_EQ(n->index.key->kind, MS_ND_INT, "key is int");
    msArenaFree(&a);
}

static void testSlice(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a[1:3:2]");
    MS_ASSERT_EQ(n->kind, MS_ND_SLICE, "slice");
    MS_ASSERT_EQ(n->slice.lo->litInt.ival,   1, "lo=1");
    MS_ASSERT_EQ(n->slice.hi->litInt.ival,   3, "hi=3");
    MS_ASSERT_EQ(n->slice.step->litInt.ival, 2, "step=2");
    msArenaFree(&a);
}

static void testSliceNoStep(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a[1:3]");
    MS_ASSERT_EQ(n->kind, MS_ND_SLICE, "slice");
    MS_ASSERT_EQ(n->slice.lo->litInt.ival, 1, "lo=1");
    MS_ASSERT_EQ(n->slice.hi->litInt.ival, 3, "hi=3");
    MS_ASSERT_TRUE(n->slice.step == NULL, "step=NULL");
    msArenaFree(&a);
}

static void testSliceAll(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a[::2]");
    MS_ASSERT_EQ(n->kind, MS_ND_SLICE, "slice");
    MS_ASSERT_TRUE(n->slice.lo == NULL, "lo=NULL");
    MS_ASSERT_TRUE(n->slice.hi == NULL, "hi=NULL");
    MS_ASSERT_EQ(n->slice.step->litInt.ival, 2, "step=2");
    msArenaFree(&a);
}

static void testPostfixInc(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "x++");
    MS_ASSERT_EQ(n->kind, MS_ND_INC_DEC, "inc_dec");
    MS_ASSERT_TRUE(n->incDec.isInc, "isInc=true");
    msArenaFree(&a);
}

static void testPostfixDec(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "x--");
    MS_ASSERT_EQ(n->kind, MS_ND_INC_DEC, "inc_dec");
    MS_ASSERT_TRUE(!n->incDec.isInc, "isInc=false");
    msArenaFree(&a);
}

static void testChainedPostfix(void) {
    struct MsArena a; msArenaInit(&a);
    // f().g[0]++  => INC_DEC( INDEX( ATTR( CALL(f), g ), 0 ) )
    MsNode* n = px(&a, "f().g[0]++");
    MS_ASSERT_EQ(n->kind,                MS_ND_INC_DEC, "outer inc_dec");
    MS_ASSERT_EQ(n->incDec.target->kind, MS_ND_INDEX,   "target is index");
    MS_ASSERT_EQ(n->incDec.target->index.obj->kind, MS_ND_ATTR, "index obj is attr");
    msArenaFree(&a);
}

// f(x==1) must parse x==1 as a positional BINARY arg, NOT a kwarg.
static void testKwargDisambiguate(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(x==1)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.args != NULL, "has positional arg");
    MS_ASSERT_EQ(n->call.args->node->kind, MS_ND_BINARY, "arg is binary (==)");
    MS_ASSERT_TRUE(n->call.kwargs == NULL, "no kwargs");
    msArenaFree(&a);
}

// f(x=1, y=2) — multiple kwargs, tests kwTail linked-list append.
static void testMultiKwarg(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "f(x=1, y=2)");
    MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
    MS_ASSERT_TRUE(n->call.kwargs != NULL, "has first kwarg");
    MS_ASSERT_EQ(n->call.kwargs->node->kind, MS_ND_KWARG_PAIR, "first kwarg pair");
    MS_ASSERT_TRUE(n->call.kwargs->next != NULL, "has second kwarg");
    MS_ASSERT_EQ(n->call.kwargs->next->node->kind, MS_ND_KWARG_PAIR, "second kwarg pair");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testEmptyCall);
    MS_RUN(testPositionalArgs);
    MS_RUN(testKwarg);
    MS_RUN(testKwargDisambiguate);
    MS_RUN(testMultiKwarg);
    MS_RUN(testStarArgs);
    MS_RUN(testTrailingComma);
    MS_RUN(testAttr);
    MS_RUN(testAttrChain);
    MS_RUN(testIndex);
    MS_RUN(testSlice);
    MS_RUN(testSliceNoStep);
    MS_RUN(testSliceAll);
    MS_RUN(testPostfixInc);
    MS_RUN(testPostfixDec);
    MS_RUN(testChainedPostfix);
    return msTestSummary();
}
