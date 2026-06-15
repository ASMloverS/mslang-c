// test_expr_binary.c
// T019: unit tests for unary/binary/power/bitwise/comparison/logical expression parsing.
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

// "1 + 2" => BINARY(PLUS, INT(1), INT(2))
static void testSimpleAdd(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "1 + 2");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,              MS_ND_BINARY,   "root binary");
    MS_ASSERT_EQ(n->binary.op,         MS_TOK_PLUS,    "root op +");
    MS_ASSERT_EQ(n->binary.left->kind, MS_ND_INT,      "left INT");
    MS_ASSERT_EQ(n->binary.right->kind,MS_ND_INT,      "right INT");
    msArenaFree(&a);
}

// "1 + 2 * 3" => + at root, * in right child (precedence)
static void testAddMulPrec(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "1 + 2 * 3");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op,              MS_TOK_PLUS, "root +");
    MS_ASSERT_EQ(n->binary.right->kind,     MS_ND_BINARY, "right binary");
    MS_ASSERT_EQ(n->binary.right->binary.op, MS_TOK_STAR, "right *");
    msArenaFree(&a);
}

// "2 ** 3 ** 2" => ** right-associative: root.right is also **
static void testPowerRightAssoc(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "2 ** 3 ** 2");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,              MS_ND_BINARY,    "root **");
    MS_ASSERT_EQ(n->binary.op,         MS_TOK_STARSTAR, "op **");
    MS_ASSERT_EQ(n->binary.right->kind,MS_ND_BINARY,    "right also **");
    MS_ASSERT_EQ(n->binary.right->binary.op, MS_TOK_STARSTAR, "right op **");
    msArenaFree(&a);
}

// "-2 ** 2" => UNARY(-, BINARY(**, INT(2), INT(2)))  i.e. -(2**2)
static void testUnaryNegPower(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "-2 ** 2");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,              MS_ND_UNARY,     "root unary");
    MS_ASSERT_EQ(n->unary.op,          MS_TOK_MINUS,    "op -");
    MS_ASSERT_EQ(n->unary.operand->kind, MS_ND_BINARY,  "operand binary");
    MS_ASSERT_EQ(n->unary.operand->binary.op, MS_TOK_STARSTAR, "inner **");
    msArenaFree(&a);
}

// "-1" => UNARY(-, INT(1))
static void testUnaryNeg(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "-1");
    MS_ASSERT_EQ(n->kind,                  MS_ND_UNARY, "unary");
    MS_ASSERT_EQ(n->unary.op,             MS_TOK_MINUS, "op -");
    MS_ASSERT_EQ(n->unary.operand->kind,  MS_ND_INT,    "operand INT");
    msArenaFree(&a);
}

// "+5" => UNARY(+, INT(5))
static void testUnaryPos(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "+5");
    MS_ASSERT_EQ(n->kind,       MS_ND_UNARY,  "unary +");
    MS_ASSERT_EQ(n->unary.op,  MS_TOK_PLUS,  "op +");
    msArenaFree(&a);
}

// "not true" => UNARY(NOT, BOOL(true))
static void testUnaryNot(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "not true");
    MS_ASSERT_EQ(n->kind,                  MS_ND_UNARY, "unary not");
    MS_ASSERT_EQ(n->unary.op,             MS_TOK_NOT,   "op not");
    MS_ASSERT_EQ(n->unary.operand->kind,  MS_ND_BOOL,   "operand BOOL");
    msArenaFree(&a);
}

// "a is not b" => BINARY(IS_NOT, IDENT(a), IDENT(b))
static void testIsNot(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a is not b");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,       MS_ND_BINARY,    "binary");
    MS_ASSERT_EQ(n->binary.op,  MS_TOK_IS_NOT,   "op is not");
    MS_ASSERT_EQ(n->binary.left->kind,  MS_ND_IDENT, "left IDENT");
    MS_ASSERT_EQ(n->binary.right->kind, MS_ND_IDENT, "right IDENT");
    msArenaFree(&a);
}

// "a not in b" => BINARY(NOT_IN, IDENT(a), IDENT(b))
static void testNotIn(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a not in b");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->kind,       MS_ND_BINARY,   "binary");
    MS_ASSERT_EQ(n->binary.op,  MS_TOK_NOT_IN,  "op not in");
    msArenaFree(&a);
}

// "a or b and c" => or at root, and(b,c) in right
static void testOrAndPrec(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a or b and c");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op,              MS_TOK_OR,  "root or");
    MS_ASSERT_EQ(n->binary.right->kind,     MS_ND_BINARY, "right binary");
    MS_ASSERT_EQ(n->binary.right->binary.op, MS_TOK_AND, "right and");
    msArenaFree(&a);
}

// "a & b | c" => | at root, &(a,b) at left
static void testBitAndOrPrec(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a & b | c");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op,             MS_TOK_PIPE, "root |");
    MS_ASSERT_EQ(n->binary.left->kind,     MS_ND_BINARY,"left binary");
    MS_ASSERT_EQ(n->binary.left->binary.op, MS_TOK_AMP, "left &");
    msArenaFree(&a);
}

// "1 < 2 < 3" => left-assoc: root.left is also <
static void testChainedCmpLeftAssoc(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "1 < 2 < 3");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op,            MS_TOK_LT,   "root <");
    MS_ASSERT_EQ(n->binary.left->kind,    MS_ND_BINARY, "left binary");
    MS_ASSERT_EQ(n->binary.left->binary.op, MS_TOK_LT, "left <");
    msArenaFree(&a);
}

// "a is b" => BINARY(IS, ...)
static void testIs(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a is b");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op, MS_TOK_IS, "op is");
    msArenaFree(&a);
}

// "a in b" => BINARY(IN, ...)
static void testIn(void) {
    struct MsArena a; msArenaInit(&a);
    MsNode* n = parseStr(&a, "a in b");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    MS_ASSERT_EQ(n->binary.op, MS_TOK_IN, "op in");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testSimpleAdd);
    MS_RUN(testAddMulPrec);
    MS_RUN(testPowerRightAssoc);
    MS_RUN(testUnaryNegPower);
    MS_RUN(testUnaryNeg);
    MS_RUN(testUnaryPos);
    MS_RUN(testUnaryNot);
    MS_RUN(testIsNot);
    MS_RUN(testNotIn);
    MS_RUN(testOrAndPrec);
    MS_RUN(testBitAndOrPrec);
    MS_RUN(testChainedCmpLeftAssoc);
    MS_RUN(testIs);
    MS_RUN(testIn);
    return msTestSummary();
}
