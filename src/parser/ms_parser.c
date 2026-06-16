// ms_parser.c
#include "mslang/ms_parser.h"

#include "parser/ms_arena.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Parse-rule table (all entries zero-initialised => {NULL, NULL, PREC_NONE})
// ---------------------------------------------------------------------------
struct ParseRule gParseRules[MS_TOK_COUNT];

_Static_assert(sizeof(gParseRules) / sizeof(gParseRules[0]) == MS_TOK_COUNT,
               "gParseRules size must equal MS_TOK_COUNT");

// ---------------------------------------------------------------------------
// Public: register a rule
// ---------------------------------------------------------------------------
void parserRegisterRule(MsTokKind kind, PrefixFn prefix, InfixFn infix,
                        Precedence prec) {
    assert((int)kind >= 0 && (int)kind < MS_TOK_COUNT);
    gParseRules[kind].prefix = prefix;
    gParseRules[kind].infix  = infix;
    gParseRules[kind].prec   = prec;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
void msParserError(MsParser* p, const char* msg) {
    if (p->panicMode) { return; }
    p->panicMode = true;
    p->hadError  = true;
    snprintf(p->errBuf, sizeof(p->errBuf), "%s:%u:%u: error: %s",
             p->cur.pos.file ? p->cur.pos.file : "?",
             p->cur.pos.line, p->cur.pos.col, msg);
}

struct MsToken msParserAdvance(MsParser* p) {
    p->prev = p->cur;
    p->cur  = msLexerNext(&p->lex);
    if (p->cur.kind == MS_TOK_ERROR && !p->panicMode) {
        msParserError(p, p->lex.errBuf);
    }
    return p->prev;
}

bool msParserCheck(MsParser* p, MsTokKind kind) {
    return p->cur.kind == kind;
}

bool msParserMatch(MsParser* p, MsTokKind kind) {
    if (!msParserCheck(p, kind)) { return false; }
    msParserAdvance(p);
    return true;
}

void msParserExpect(MsParser* p, MsTokKind kind, const char* msg) {
    if (msParserCheck(p, kind)) {
        msParserAdvance(p);
        return;
    }
    msParserError(p, msg);
}

struct MsToken msParserPeekNext(MsParser* p) {
    return msLexerPeek(&p->lex);
}

void msParserSyncError(MsParser* p) {
    p->panicMode = false;
    while (p->cur.kind != MS_TOK_EOF) {
        if (p->cur.kind == MS_TOK_NEWLINE ||
            p->cur.kind == MS_TOK_SEMICOLON) {
            msParserAdvance(p);
            return;
        }
        switch (p->cur.kind) {
            case MS_TOK_FUNC:
            case MS_TOK_CLASS:
            case MS_TOK_FOR:
            case MS_TOK_IF:
            case MS_TOK_RETURN:
            case MS_TOK_VAR:
                return;
            default:
                break;
        }
        msParserAdvance(p);
    }
}

// ---------------------------------------------------------------------------
// Public: init
// ---------------------------------------------------------------------------
void msParserInit(MsParser* p, const char* src, uint32_t srcLen,
                  const char* fileName, struct MsArena* arena) {
    msParseExprRegisterRules();
    msLexerInit(&p->lex, src, srcLen, fileName);
    p->arena    = arena;
    p->hadError = false;
    p->panicMode = false;
    p->errBuf[0] = '\0';
    p->cur  = msLexerNext(&p->lex);
    p->prev = (struct MsToken){0};
}

// ---------------------------------------------------------------------------
// Core Pratt loop
// ---------------------------------------------------------------------------
MsNode* parsePrecedence(MsParser* p, Precedence minPrec) {
    msParserAdvance(p);
    struct ParseRule* rule = &gParseRules[p->prev.kind];
    if (rule->prefix == NULL) {
        msParserError(p, "expected expression");
        return NULL;
    }
    MsNode* left = rule->prefix(p);

    while (!p->panicMode) {
        struct ParseRule* cur = &gParseRules[p->cur.kind];
        if (cur->prec < minPrec) { break; }
        msParserAdvance(p);
        if (cur->infix == NULL) {
            msParserError(p, "no infix rule for operator");
            break;
        }
        left = cur->infix(p, left);
    }
    return left;
}

MsNode* msParseExpr(MsParser* p) {
    return parsePrecedence(p, PREC_IF_EXPR);
}

// ---------------------------------------------------------------------------
// Statement / program stubs (expanded by T019+)
// ---------------------------------------------------------------------------
MsNode* msParseStmt(MsParser* p) {
    MsNode* expr = msParseExpr(p);
    msParserMatch(p, MS_TOK_NEWLINE);
    msParserMatch(p, MS_TOK_SEMICOLON);
    if (p->hadError) { msParserSyncError(p); }

    if (expr == NULL) { return NULL; }
    MsNode* stmt = MS_ARENA_NEW(p->arena, MsNode);
    stmt->kind         = MS_ND_EXPR_STMT;
    stmt->pos          = expr->pos;
    stmt->exprStmt.expr = expr;
    return stmt;
}

MsNode* msParseProgram(MsParser* p) {
    MsNode* prog = MS_ARENA_NEW(p->arena, MsNode);
    prog->kind = MS_ND_PROGRAM;
    prog->pos  = p->cur.pos;
    prog->program.filename = p->lex.fileName;
    prog->program.stmts    = NULL;

    MsNodeList** tail = &prog->program.stmts;

    while (!msParserCheck(p, MS_TOK_EOF)) {
        if (msParserMatch(p, MS_TOK_NEWLINE) ||
            msParserMatch(p, MS_TOK_SEMICOLON)) {
            continue;
        }
        MsNode* stmt = msParseStmt(p);
        if (stmt == NULL) { continue; }
        MsNodeList* entry = MS_ARENA_NEW(p->arena, MsNodeList);
        entry->node = stmt;
        entry->next = NULL;
        *tail = entry;
        tail  = &entry->next;
    }
    return prog;
}
