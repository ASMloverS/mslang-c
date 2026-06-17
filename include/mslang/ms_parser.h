// ms_parser.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_ast.h"
#include "mslang/ms_lexer.h"

// Forward declaration — avoids pulling in internal src/parser/ms_arena.h.
struct MsArena;

// ---------------------------------------------------------------------------
// Precedence levels (Pratt TDOP)
// ---------------------------------------------------------------------------
typedef enum Precedence {
  PREC_NONE = 0,
  PREC_IF_EXPR = 1,  // a if cond else b
  PREC_OR = 2,       // or
  PREC_AND = 3,      // and
  PREC_NOT = 4,      // not (unary prefix)
  PREC_COMPARE = 5,  // == != < > <= >= is in
  PREC_BITOR = 6,    // |
  PREC_BITXOR = 7,   // ^
  PREC_BITAND = 8,   // &
  PREC_SHIFT = 9,    // << >>
  PREC_TERM = 10,    // + -
  PREC_FACTOR = 11,  // * / // %
  PREC_UNARY = 12,   // - ~ + (unary prefix)
  PREC_POWER = 13,   // ** (right-assoc)
  PREC_CALL = 14,    // () [] . (postfix)
  PREC_COUNT = 15,   // sentinel
} Precedence;

// ---------------------------------------------------------------------------
// Parser structure
// ---------------------------------------------------------------------------
typedef struct MsParser {
  struct MsLexer lex;     // embedded lexer (owns the lexer)
  struct MsToken cur;     // current token
  struct MsToken prev;    // previous token
  struct MsArena* arena;  // AST memory (caller-owned)
  bool hadError;          // parse error occurred
  bool panicMode;         // suppresses cascading errors
  char errBuf[256];       // most recent error message
} MsParser;

// ---------------------------------------------------------------------------
// Pratt parse-rule table
// ---------------------------------------------------------------------------
typedef MsNode* (*PrefixFn)(MsParser*);
typedef MsNode* (*InfixFn)(MsParser*, MsNode* left);

struct ParseRule {
  PrefixFn prefix;
  InfixFn infix;
  Precedence prec;
};

// One entry per token kind; size validated by _Static_assert in ms_parser.c.
extern struct ParseRule gParseRules[MS_TOK_COUNT];

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// src must remain valid for the parser's lifetime.
void msParserInit(MsParser* p, const char* src, uint32_t srcLen, const char* fileName, struct MsArena* arena);

void parserRegisterRule(MsTokKind kind, PrefixFn prefix, InfixFn infix, Precedence prec);

// Register all T019 expression parse rules into gParseRules.
// Called once by msParserInit; safe to call again (idempotent overwrite).
void msParseExprRegisterRules(void);

MsNode* parsePrecedence(MsParser* p, Precedence minPrec);
MsNode* msParseExpr(MsParser* p);
MsNode* parseMaybeTuple(MsParser* p, MsNode* first);
MsNode* msParseStmt(MsParser* p);
MsNode* msParseProgram(MsParser* p);
MsNodeList* msParseParamList(MsParser* p);

// ---------------------------------------------------------------------------
// Internal helpers exposed for testing
// ---------------------------------------------------------------------------
struct MsToken msParserAdvance(MsParser* p);
bool msParserCheck(MsParser* p, MsTokKind kind);
bool msParserMatch(MsParser* p, MsTokKind kind);
void msParserExpect(MsParser* p, MsTokKind kind, const char* msg);
void msParserSyncError(MsParser* p);
void msParserError(MsParser* p, const char* msg);
struct MsToken msParserPeekNext(MsParser* p);
