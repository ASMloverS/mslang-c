// test_func_literal.c
// T024: unit tests for function literal / anonymous function parsing.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* px(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  return msParseExpr(&p);
}

static bool pxErr(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t) strlen(s), "<t>", a);
  msParseExpr(&p);
  return p.hadError;
}

static int listLen(MsNodeList* l) {
  int n = 0;
  while (l) {
    n++;
    l = l->next;
  }
  return n;
}

static void testEmptyFuncLit(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func() {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.name == NULL, "anonymous");
  MS_ASSERT_TRUE(n->funcDecl.params == NULL, "no params");
  MS_ASSERT_TRUE(n->funcDecl.body != NULL, "has body");
  MS_ASSERT_EQ(n->funcDecl.body->kind, MS_ND_BLOCK, "body is block");
  MS_ASSERT_TRUE(n->funcDecl.isAsync == false, "not async");
  msArenaFree(&a);
}

static void testFuncWithParams(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func(a, b) { return a }");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  MS_ASSERT_TRUE(n->funcDecl.name == NULL, "anonymous");
  MS_ASSERT_EQ(listLen(n->funcDecl.params), 2, "2 params");
  MS_ASSERT_TRUE(n->funcDecl.body != NULL, "has body");
  msArenaFree(&a);
}

static void testFuncDefaultParam(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func(a, b=1) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  MS_ASSERT_EQ(listLen(n->funcDecl.params), 2, "2 params");
  MsNode* paramB = n->funcDecl.params->next->node;
  MS_ASSERT_EQ(paramB->kind, MS_ND_PARAM, "param node");
  MS_ASSERT_TRUE(paramB->param.defaultVal != NULL, "has default");
  MS_ASSERT_EQ(paramB->param.defaultVal->kind, MS_ND_INT, "default is int");
  MS_ASSERT_EQ(paramB->param.defaultVal->litInt.ival, 1, "default == 1");
  msArenaFree(&a);
}

static void testFuncVararg(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func(first, ...args) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  MS_ASSERT_EQ(listLen(n->funcDecl.params), 2, "2 params");
  MsNode* p2 = n->funcDecl.params->next->node;
  MS_ASSERT_TRUE(p2->param.isVararg, "isVararg");
  msArenaFree(&a);
}

static void testFuncKwarg(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func(first, **kw) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  MS_ASSERT_EQ(listLen(n->funcDecl.params), 2, "2 params");
  MsNode* p2 = n->funcDecl.params->next->node;
  MS_ASSERT_TRUE(p2->param.isKwarg, "isKwarg");
  msArenaFree(&a);
}

static void testFuncAllParamKinds(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "func(a, ...args, **kw) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  MS_ASSERT_EQ(listLen(n->funcDecl.params), 3, "3 params");
  MsNode* p1 = n->funcDecl.params->node;
  MsNode* p2 = n->funcDecl.params->next->node;
  MsNode* p3 = n->funcDecl.params->next->next->node;
  MS_ASSERT_TRUE(!p1->param.isVararg && !p1->param.isKwarg, "p1 normal");
  MS_ASSERT_TRUE(p2->param.isVararg, "p2 vararg");
  MS_ASSERT_TRUE(p3->param.isKwarg, "p3 kwarg");
  msArenaFree(&a);
}

static void testAsyncFuncLit(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = px(&a, "async func() {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.isAsync == true, "is async");
  MS_ASSERT_TRUE(n->funcDecl.name == NULL, "anonymous");
  msArenaFree(&a);
}

static void testErrorNoDefaultAfterDefault(void) {
  struct MsArena a;
  msArenaInit(&a);
  bool err = pxErr(&a, "func(b=1, a) {}");
  MS_ASSERT_TRUE(err, "non-default after default is error");
  msArenaFree(&a);
}

static void testErrorVarargNoLeading(void) {
  struct MsArena a;
  msArenaInit(&a);
  bool err = pxErr(&a, "func(...args) {}");
  MS_ASSERT_TRUE(err, "vararg with no leading param is error");
  msArenaFree(&a);
}

static void testErrorVarargAfterKwarg(void) {
  struct MsArena a;
  msArenaInit(&a);
  bool err = pxErr(&a, "func(a, **kw, ...args) {}");
  MS_ASSERT_TRUE(err, "...args after **kwargs is error");
  msArenaFree(&a);
}

static void testErrorPositionalAfterKwarg(void) {
  struct MsArena a;
  msArenaInit(&a);
  bool err = pxErr(&a, "func(a, **kw, b) {}");
  MS_ASSERT_TRUE(err, "positional after **kwargs is error");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testEmptyFuncLit);
  MS_RUN(testFuncWithParams);
  MS_RUN(testFuncDefaultParam);
  MS_RUN(testFuncVararg);
  MS_RUN(testFuncKwarg);
  MS_RUN(testFuncAllParamKinds);
  MS_RUN(testAsyncFuncLit);
  MS_RUN(testErrorNoDefaultAfterDefault);
  MS_RUN(testErrorVarargNoLeading);
  MS_RUN(testErrorVarargAfterKwarg);
  MS_RUN(testErrorPositionalAfterKwarg);
  return msTestSummary();
}
