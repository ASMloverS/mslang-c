// test_container_literals.c
// T022: unit tests for list/set/map literal parsing.
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

static int listLen(MsNodeList* l) {
    int n = 0;
    while (l) { n++; l = l->next; }
    return n;
}

static void testEmptyList(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "[]");
    MS_ASSERT_EQ(n->kind, MS_ND_LIST, "empty list kind");
    MS_ASSERT_TRUE(n->container.elems == NULL, "no elems");
    msArenaFree(&a);
}

static void testList123(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "[1, 2, 3]");
    MS_ASSERT_EQ(n->kind, MS_ND_LIST, "list kind");
    MS_ASSERT_EQ(listLen(n->container.elems), 3, "3 elems");
    msArenaFree(&a);
}

static void testListTrailingComma(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "[1,]");
    MS_ASSERT_EQ(n->kind, MS_ND_LIST, "list kind");
    MS_ASSERT_EQ(listLen(n->container.elems), 1, "1 elem");
    msArenaFree(&a);
}

static void testEmptyMap(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{}");
    MS_ASSERT_EQ(n->kind, MS_ND_MAP, "empty map kind");
    MS_ASSERT_TRUE(n->map.pairs == NULL, "no pairs");
    msArenaFree(&a);
}

static void testMap(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{\"a\": 1, \"b\": 2}");
    MS_ASSERT_EQ(n->kind, MS_ND_MAP, "map kind");
    MS_ASSERT_EQ(listLen(n->map.pairs), 2, "2 pairs");
    msArenaFree(&a);
}

static void testSet(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{1, 2, 3}");
    MS_ASSERT_EQ(n->kind, MS_ND_SET, "set kind");
    MS_ASSERT_EQ(listLen(n->container.elems), 3, "3 elems");
    msArenaFree(&a);
}

static void testSingleElemSet(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{1}");
    MS_ASSERT_EQ(n->kind, MS_ND_SET, "single set kind");
    MS_ASSERT_EQ(listLen(n->container.elems), 1, "1 elem");
    msArenaFree(&a);
}

static void testMapTrailingComma(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{\"a\": 1,}");
    MS_ASSERT_EQ(n->kind, MS_ND_MAP, "map trailing comma kind");
    MS_ASSERT_EQ(listLen(n->map.pairs), 1, "1 pair");
    msArenaFree(&a);
}

static void testSetTrailingComma(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{1,}");
    MS_ASSERT_EQ(n->kind, MS_ND_SET, "set trailing comma kind");
    MS_ASSERT_EQ(listLen(n->container.elems), 1, "1 elem");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testEmptyList);
    MS_RUN(testList123);
    MS_RUN(testListTrailingComma);
    MS_RUN(testEmptyMap);
    MS_RUN(testMap);
    MS_RUN(testSet);
    MS_RUN(testSingleElemSet);
    MS_RUN(testMapTrailingComma);
    MS_RUN(testSetTrailingComma);
    return msTestSummary();
}
