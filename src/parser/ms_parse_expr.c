// ms_parse_expr.c
// T019: prefix/infix parse functions for literals, unary, and binary operators.
// Registers all rules into gParseRules via msParseExprRegisterRules().
#include "mslang/ms_parser.h"

#include "parser/ms_arena.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static MsNode* parseUnary(MsParser* p);
static MsNode* parseBinary(MsParser* p, MsNode* left);
static MsNode* parseIsIn(MsParser* p, MsNode* left);

// ---------------------------------------------------------------------------
// Literal prefix parsers
// ---------------------------------------------------------------------------
static MsNode* parseIntLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind        = MS_ND_INT;
    n->pos         = p->prev.pos;
    n->litInt.ival = p->prev.val.ival;
    return n;
}

static MsNode* parseFloatLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind          = MS_ND_FLOAT;
    n->pos           = p->prev.pos;
    n->litFloat.fval = p->prev.val.fval;
    return n;
}

static MsNode* parseStringLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind         = MS_ND_STRING;
    n->pos          = p->prev.pos;
    n->litStr.data  = p->prev.start;
    n->litStr.len   = p->prev.len;
    return n;
}

static MsNode* parseBytesLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind        = MS_ND_BYTES;
    n->pos         = p->prev.pos;
    n->litStr.data = p->prev.start;
    n->litStr.len  = p->prev.len;
    return n;
}

static MsNode* parseTrueLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind          = MS_ND_BOOL;
    n->pos           = p->prev.pos;
    n->litBool.bval  = true;
    return n;
}

static MsNode* parseFalseLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind         = MS_ND_BOOL;
    n->pos          = p->prev.pos;
    n->litBool.bval = false;
    return n;
}

static MsNode* parseNilLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_NIL;
    n->pos  = p->prev.pos;
    return n;
}

static MsNode* parseIdentLit(MsParser* p) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind       = MS_ND_IDENT;
    n->pos        = p->prev.pos;
    n->ident.name = p->prev.start;
    n->ident.len  = p->prev.len;
    return n;
}

// ---------------------------------------------------------------------------
// Unary prefix parser  (-x  +x  ~x  not x)
// ---------------------------------------------------------------------------
static MsNode* parseUnary(MsParser* p) {
    MsTokKind  op  = p->prev.kind;
    struct MsSrcPos pos = p->prev.pos;
    // 'not' is a logical negation with lower precedence than power.
    // '-', '+', '~' must bind tighter than '**' so that -2**2 == -(2**2).
    Precedence prec = (op == MS_TOK_NOT) ? PREC_NOT : PREC_POWER;
    MsNode* operand = parsePrecedence(p, prec);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind          = MS_ND_UNARY;
    n->pos           = pos;
    n->unary.op      = op;
    n->unary.operand = operand;
    return n;
}

// ---------------------------------------------------------------------------
// Binary infix parser (arithmetic, bit, comparison, logic)
// ---------------------------------------------------------------------------
static MsNode* parseBinary(MsParser* p, MsNode* left) {
    MsTokKind  op  = p->prev.kind;
    struct MsSrcPos pos = p->prev.pos;
    Precedence prec = gParseRules[op].prec;
    // '**' is right-associative: recurse at same precedence level.
    bool rightAssoc = (op == MS_TOK_STARSTAR);
    MsNode* right = parsePrecedence(p, rightAssoc ? prec : (Precedence)(prec + 1));
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind          = MS_ND_BINARY;
    n->pos           = pos;
    n->binary.op     = op;
    n->binary.left   = left;
    n->binary.right  = right;
    return n;
}

// ---------------------------------------------------------------------------
// 'is [not]' / '[not] in' / 'in'  infix parser
// ---------------------------------------------------------------------------
static MsNode* parseIsIn(MsParser* p, MsNode* left) {
    MsTokKind op  = p->prev.kind;  // MS_TOK_IS, MS_TOK_IN, or MS_TOK_NOT
    struct MsSrcPos pos = p->prev.pos;

    if (op == MS_TOK_IS && msParserMatch(p, MS_TOK_NOT)) {
        op = MS_TOK_IS_NOT;
    } else if (op == MS_TOK_NOT) {
        msParserExpect(p, MS_TOK_IN, "'in' expected after 'not'");
        op = MS_TOK_NOT_IN;
    }
    // Right operand at PREC_COMPARE + 1 (left-associative, no chaining)
    MsNode* right = parsePrecedence(p, (Precedence)(PREC_COMPARE + 1));
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind         = MS_ND_BINARY;
    n->pos          = pos;
    n->binary.op    = op;
    n->binary.left  = left;
    n->binary.right = right;
    return n;
}

// ---------------------------------------------------------------------------
// Rule registration (called once at startup or from test harness)
// ---------------------------------------------------------------------------
void msParseExprRegisterRules(void) {
    // Literals
    parserRegisterRule(MS_TOK_INT,    parseIntLit,    NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_FLOAT,  parseFloatLit,  NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_STRING, parseStringLit, NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_BYTES,  parseBytesLit,  NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_TRUE,   parseTrueLit,   NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_FALSE,  parseFalseLit,  NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_NIL,    parseNilLit,    NULL,        PREC_NONE);
    parserRegisterRule(MS_TOK_IDENT,  parseIdentLit,  NULL,        PREC_NONE);

    // Unary prefix  (+ and - also serve as infix with parseBinary)
    parserRegisterRule(MS_TOK_MINUS, parseUnary,  parseBinary, PREC_TERM);
    parserRegisterRule(MS_TOK_PLUS,  parseUnary,  parseBinary, PREC_TERM);
    parserRegisterRule(MS_TOK_TILDE, parseUnary,  NULL,        PREC_NONE);
    // 'not': prefix = parseUnary, infix = parseIsIn (for "not in")
    parserRegisterRule(MS_TOK_NOT,   parseUnary,  parseIsIn,   PREC_COMPARE);

    // Arithmetic binary
    parserRegisterRule(MS_TOK_STAR,     NULL, parseBinary, PREC_FACTOR);
    parserRegisterRule(MS_TOK_SLASH,    NULL, parseBinary, PREC_FACTOR);
    parserRegisterRule(MS_TOK_PERCENT,  NULL, parseBinary, PREC_FACTOR);
    parserRegisterRule(MS_TOK_STARSTAR, NULL, parseBinary, PREC_POWER);

    // Bitwise binary
    parserRegisterRule(MS_TOK_SHL,   NULL, parseBinary, PREC_SHIFT);
    parserRegisterRule(MS_TOK_SHR,   NULL, parseBinary, PREC_SHIFT);
    parserRegisterRule(MS_TOK_AMP,   NULL, parseBinary, PREC_BITAND);
    parserRegisterRule(MS_TOK_PIPE,  NULL, parseBinary, PREC_BITOR);
    parserRegisterRule(MS_TOK_CARET, NULL, parseBinary, PREC_BITXOR);

    // Comparison
    parserRegisterRule(MS_TOK_EQ,  NULL, parseBinary, PREC_COMPARE);
    parserRegisterRule(MS_TOK_NEQ, NULL, parseBinary, PREC_COMPARE);
    parserRegisterRule(MS_TOK_LT,  NULL, parseBinary, PREC_COMPARE);
    parserRegisterRule(MS_TOK_GT,  NULL, parseBinary, PREC_COMPARE);
    parserRegisterRule(MS_TOK_LE,  NULL, parseBinary, PREC_COMPARE);
    parserRegisterRule(MS_TOK_GE,  NULL, parseBinary, PREC_COMPARE);

    // 'is' and 'in'
    parserRegisterRule(MS_TOK_IS, NULL, parseIsIn, PREC_COMPARE);
    parserRegisterRule(MS_TOK_IN, NULL, parseIsIn, PREC_COMPARE);

    // Logical
    parserRegisterRule(MS_TOK_AND, NULL, parseBinary, PREC_AND);
    parserRegisterRule(MS_TOK_OR,  NULL, parseBinary, PREC_OR);
}
