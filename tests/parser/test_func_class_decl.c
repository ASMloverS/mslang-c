// test_func_class_decl.c
// T034: unit tests for func/class declaration parsing.
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

static void testFuncDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "func add(a, b) { return a + b }");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.name != NULL, "named");
  MS_ASSERT_TRUE(n->funcDecl.isAsync == false, "not async");
  int cnt = 0;
  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) {
    cnt++;
  }
  MS_ASSERT_EQ(cnt, 2, "2 params");
  msArenaFree(&a);
}

static void testFuncDeclNoParams(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "func f() {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.name != NULL, "named");
  MS_ASSERT_TRUE(n->funcDecl.params == NULL, "no params");
  MS_ASSERT_EQ(n->funcDecl.body->kind, MS_ND_BLOCK, "body block");
  msArenaFree(&a);
}

static void testFuncDeclWithDefaults(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "func f(a, b=1, ...args, **kw) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  int cnt = 0;
  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) {
    cnt++;
  }
  MS_ASSERT_EQ(cnt, 4, "4 params");
  msArenaFree(&a);
}

static void testAsyncFuncDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "async func f() {}");
  MS_ASSERT_EQ(n->kind, MS_ND_ASYNC_FUNC, "async func");
  MS_ASSERT_TRUE(n->funcDecl.isAsync == true, "is async");
  MS_ASSERT_TRUE(n->funcDecl.name != NULL, "named");
  msArenaFree(&a);
}

static void testClassDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Foo {}");
  MS_ASSERT_EQ(n->kind, MS_ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->classDecl.name != NULL, "named");
  MS_ASSERT_TRUE(n->classDecl.base == NULL, "no base");
  MS_ASSERT_TRUE(n->classDecl.body == NULL, "empty body");
  msArenaFree(&a);
}

static void testClassExtends(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Foo extends Bar {}");
  MS_ASSERT_EQ(n->kind, MS_ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->classDecl.base != NULL, "has base");
  MS_ASSERT_EQ(n->classDecl.base->kind, MS_ND_IDENT, "base ident");
  msArenaFree(&a);
}

static void testClassWithMethod(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Point extends Base { func __init__(self) { } }");
  MS_ASSERT_EQ(n->kind, MS_ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->classDecl.base != NULL, "has base");
  MS_ASSERT_TRUE(n->classDecl.body != NULL, "has body");
  MS_ASSERT_EQ(n->classDecl.body->node->kind, MS_ND_FUNC_DECL, "method");
  msArenaFree(&a);
}

static void testFuncDefaultParamError(void) {
  struct MsArena a;
  msArenaInit(&a);
  MS_ASSERT_TRUE(psErr(&a, "func f(b=1, a) {}"), "non-default after default");
  msArenaFree(&a);
}

static void testFuncDefaultVal(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "func f(a=1+2) {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MsNode* param = n->funcDecl.params->node;
  MS_ASSERT_EQ(param->kind, MS_ND_PARAM, "param node");
  MS_ASSERT_TRUE(param->param.defaultVal != NULL, "has default");
  MS_ASSERT_EQ(param->param.defaultVal->kind, MS_ND_BINARY, "default is binary");
  msArenaFree(&a);
}

static void testClassWithAttr(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Foo { x := 1 }");
  MS_ASSERT_EQ(n->kind, MS_ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->classDecl.body != NULL, "has body");
  MS_ASSERT_EQ(n->classDecl.body->node->kind, MS_ND_SHORT_DECL, "attr");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testFuncDecl);
  MS_RUN(testFuncDeclNoParams);
  MS_RUN(testFuncDeclWithDefaults);
  MS_RUN(testAsyncFuncDecl);
  MS_RUN(testClassDecl);
  MS_RUN(testClassExtends);
  MS_RUN(testClassWithMethod);
  MS_RUN(testClassWithAttr);
  MS_RUN(testFuncDefaultParamError);
  MS_RUN(testFuncDefaultVal);
  return msTestSummary();
}
