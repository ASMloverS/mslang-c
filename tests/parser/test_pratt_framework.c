// test_pratt_framework.c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "parser/ms_arena.h"

static void parserFromStr(MsParser* p, struct MsArena* arena,
                           const char* src) {
    msParserInit(p, src, (uint32_t)strlen(src), "<t>", arena);
}

// T018-1: empty input => hadError or NULL
static void testEmptyExpr(void) {
    struct MsArena arena;
    msArenaInit(&arena);
    MsParser p;
    parserFromStr(&p, &arena, "");
    MsNode* n = msParseExpr(&p);
    MS_ASSERT_TRUE(p.hadError || n == NULL, "empty input => error or null");
    msArenaFree(&arena);
}

// T018-2: parserRegisterRule + literal prefix => parsePrecedence returns node
static MsNode* prefixInt(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_INT;
    n->litInt.ival = p->prev.val.ival;
    n->pos = p->prev.pos;
    return n;
}

static void testIntLiteralParsed(void) {
    parserRegisterRule(MS_TOK_INT, prefixInt, NULL, PREC_NONE);

    struct MsArena arena;
    msArenaInit(&arena);
    MsParser p;
    parserFromStr(&p, &arena, "42");
    MsNode* n = msParseExpr(&p);
    MS_ASSERT_FALSE(p.hadError, "no error for '42'");
    MS_ASSERT_TRUE(n != NULL, "node non-null");
    if (n) {
        MS_ASSERT_EQ(n->kind, MS_ND_INT, "kind == MS_ND_INT");
        MS_ASSERT_EQ(n->litInt.ival, 42, "ival == 42");
    }
    msArenaFree(&arena);
}

// T018-4: MS_TOK_NEWLINE prec == PREC_NONE (breaks infix loop naturally)
static void testNewlinePrec(void) {
    MS_ASSERT_EQ(gParseRules[MS_TOK_NEWLINE].prec, PREC_NONE,
                 "NEWLINE prec == PREC_NONE");
}

// T018-5: match(NEWLINE) and match(SEMICOLON) both consume the token.
// ASI inserts NEWLINE after tokens like an identifier; the parser must handle
// both NEWLINE and SEMICOLON as statement separators.
static void testMatchNewlineAndSemicolon(void) {
    struct MsArena arena;
    msArenaInit(&arena);

    // "x\n" — lexer emits IDENT then ASI-NEWLINE.
    MsParser p1;
    parserFromStr(&p1, &arena, "x\n");
    // cur == IDENT 'x'; advance past it, then cur == NEWLINE
    msParserAdvance(&p1);
    bool gotNL = msParserMatch(&p1, MS_TOK_NEWLINE);
    MS_ASSERT_TRUE(gotNL, "match(NEWLINE) after ident");

    // ";" — lexer emits SEMICOLON directly.
    MsParser p2;
    parserFromStr(&p2, &arena, ";");
    bool gotSC = msParserMatch(&p2, MS_TOK_SEMICOLON);
    MS_ASSERT_TRUE(gotSC, "match(SEMICOLON) on ';'");

    msArenaFree(&arena);
}

// T018-6: panic mode — error sets hadError; syncError skips to next newline
static void testPanicModeError(void) {
    struct MsArena arena;
    msArenaInit(&arena);
    MsParser p;
    parserFromStr(&p, &arena, "+ 1");
    MsNode* n = msParseExpr(&p);
    MS_ASSERT_TRUE(p.hadError, "hadError set on unexpected '+'");
    (void)n;
    msArenaFree(&arena);
}

// Infix helper for testPanicModeRecovery
static MsNode* infixAdd(MsParser* p, MsNode* left) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_BINARY;
    n->pos  = p->prev.pos;
    n->binary.op    = MS_TOK_PLUS;
    n->binary.left  = left;
    n->binary.right = parsePrecedence(p, PREC_TERM + 1);
    return n;
}

// T018-7: after error + sync, a following binary expression still parses
static void testPanicModeRecovery(void) {
    parserRegisterRule(MS_TOK_INT,  prefixInt, NULL,     PREC_NONE);
    parserRegisterRule(MS_TOK_PLUS, NULL,      infixAdd, PREC_TERM);

    struct MsArena arena;
    msArenaInit(&arena);

    // Input: bad expr, then semicolon as sync boundary, then "1 + 2".
    // parsePrecedence on "+" sets panicMode+hadError, returns NULL.
    // msParserSyncError sees the semicolon, advances past it, and returns.
    // The second parsePrecedence call must then parse "1 + 2" successfully.
    MsParser p;
    parserFromStr(&p, &arena, "+ ; 1 + 2");

    MsNode* bad = parsePrecedence(&p, PREC_IF_EXPR);
    MS_ASSERT_TRUE(p.hadError, "hadError after bad expr");
    MS_ASSERT_TRUE(p.panicMode, "panicMode after bad expr");
    (void)bad;

    msParserSyncError(&p);
    MS_ASSERT_FALSE(p.panicMode, "panicMode cleared after sync");

    MsNode* n = parsePrecedence(&p, PREC_IF_EXPR);
    MS_ASSERT_TRUE(n != NULL, "second expr non-null after recovery");
    if (n) {
        MS_ASSERT_EQ(n->kind, MS_ND_BINARY, "second expr is binary");
    }
    msArenaFree(&arena);
}

int main(void) {
    MS_RUN(testEmptyExpr);
    MS_RUN(testIntLiteralParsed);
    MS_RUN(testNewlinePrec);
    MS_RUN(testMatchNewlineAndSemicolon);
    MS_RUN(testPanicModeError);
    MS_RUN(testPanicModeRecovery);
    return msTestSummary();
}
