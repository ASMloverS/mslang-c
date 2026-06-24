// test_import.c
// T035: unit tests for import statement parsing.
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

static void testImportBasic(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "import math");
  MS_ASSERT_EQ(n->kind, MS_ND_IMPORT, "import");
  MS_ASSERT_TRUE(!n->importStmt.fromImport, "not from-import");
  MS_ASSERT_TRUE(n->importStmt.path != NULL, "has path");
  MS_ASSERT_TRUE(n->importStmt.asName == NULL, "no alias");
  msArenaFree(&a);
}

static void testImportDotted(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "import foo.bar");
  MS_ASSERT_EQ(n->kind, MS_ND_IMPORT, "import");
  MS_ASSERT_TRUE(n->importStmt.path != NULL, "has path");
  MS_ASSERT_TRUE(n->importStmt.path->next != NULL, "dotted path");
  msArenaFree(&a);
}

static void testImportAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "import foo as f");
  MS_ASSERT_EQ(n->kind, MS_ND_IMPORT, "import");
  MS_ASSERT_TRUE(n->importStmt.asName != NULL, "has alias");
  msArenaFree(&a);
}

static void testImportDottedAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "import foo.bar as fb");
  MS_ASSERT_EQ(n->kind, MS_ND_IMPORT, "import");
  MS_ASSERT_TRUE(n->importStmt.path != NULL, "has path");
  MS_ASSERT_TRUE(n->importStmt.path->next != NULL, "dotted");
  MS_ASSERT_TRUE(n->importStmt.asName != NULL, "has alias");
  MS_ASSERT_TRUE(!n->importStmt.fromImport, "not from");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testImportBasic);
  MS_RUN(testImportDotted);
  MS_RUN(testImportAs);
  MS_RUN(testImportDottedAs);
  return msTestSummary();
}
