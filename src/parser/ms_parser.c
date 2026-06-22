// ms_parser.c
#include "mslang/ms_parser.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "parser/ms_arena.h"

// ---------------------------------------------------------------------------
// Parse-rule table (all entries zero-initialised => {NULL, NULL, PREC_NONE})
// ---------------------------------------------------------------------------
struct ParseRule gParseRules[MS_TOK_COUNT];

_Static_assert(
    sizeof(gParseRules) / sizeof(gParseRules[0]) == MS_TOK_COUNT, "gParseRules size must equal MS_TOK_COUNT");

// ---------------------------------------------------------------------------
// Public: register a rule
// ---------------------------------------------------------------------------
void parserRegisterRule(MsTokKind kind, PrefixFn prefix, InfixFn infix, Precedence prec) {
  assert((int) kind >= 0 && (int) kind < MS_TOK_COUNT);
  gParseRules[kind].prefix = prefix;
  gParseRules[kind].infix = infix;
  gParseRules[kind].prec = prec;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
void msParserError(MsParser* p, const char* msg) {
  if (p->panicMode) {
    return;
  }
  p->panicMode = true;
  p->hadError = true;
  snprintf(
      p->errBuf,
      sizeof(p->errBuf),
      "%s:%u:%u: error: %s",
      p->cur.pos.file ? p->cur.pos.file : "?",
      p->cur.pos.line,
      p->cur.pos.col,
      msg);
}

struct MsToken msParserAdvance(MsParser* p) {
  p->prev = p->cur;
  p->cur = msLexerNext(&p->lex);
  if (p->cur.kind == MS_TOK_ERROR && !p->panicMode) {
    msParserError(p, p->lex.errBuf);
  }
  return p->prev;
}

bool msParserCheck(MsParser* p, MsTokKind kind) {
  return p->cur.kind == kind;
}

bool msParserMatch(MsParser* p, MsTokKind kind) {
  if (!msParserCheck(p, kind)) {
    return false;
  }
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
    if (p->cur.kind == MS_TOK_NEWLINE || p->cur.kind == MS_TOK_SEMICOLON) {
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
// Append node to a singly-linked MsNodeList via tail pointer.
// ---------------------------------------------------------------------------
void msNodeListAppend(MsParser* p, MsNodeList*** tail, MsNode* node) {
  MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
  item->node = node;
  item->next = NULL;
  **tail = item;
  *tail = &item->next;
}

// ---------------------------------------------------------------------------
// Public: init
// ---------------------------------------------------------------------------
void msParserInit(MsParser* p, const char* src, uint32_t srcLen, const char* fileName, struct MsArena* arena) {
  msParseExprRegisterRules();
  msLexerInit(&p->lex, src, srcLen, fileName);
  p->arena = arena;
  p->hadError = false;
  p->panicMode = false;
  p->errBuf[0] = '\0';
  p->cur = msLexerNext(&p->lex);
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
    if (cur->prec < minPrec) {
      break;
    }
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
// Statement helpers
// ---------------------------------------------------------------------------
static bool isCompoundAssign(MsTokKind k) {
  switch (k) {
    case MS_TOK_PLUS_ASSIGN:
    case MS_TOK_MINUS_ASSIGN:
    case MS_TOK_STAR_ASSIGN:
    case MS_TOK_SLASH_ASSIGN:
    case MS_TOK_PERCENT_ASSIGN:
    case MS_TOK_AMP_ASSIGN:
    case MS_TOK_PIPE_ASSIGN:
    case MS_TOK_CARET_ASSIGN:
    case MS_TOK_SHL_ASSIGN:
    case MS_TOK_SHR_ASSIGN:
      return true;
    default:
      return false;
  }
}

static MsNode* parseVarDecl(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_IDENT, "expected variable name");

  const char* name = p->prev.start;
  uint32_t nameLen = p->prev.len;

  MsNode* init = NULL;
  if (msParserMatch(p, MS_TOK_ASSIGN)) {
    init = msParseExpr(p);
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_VAR_DECL;
  n->pos = pos;
  n->varDecl.name = name;
  n->varDecl.nameLen = nameLen;
  n->varDecl.init = init;
  return n;
}

static void finishStmt(MsParser* p) {
  msParserMatch(p, MS_TOK_NEWLINE);
  msParserMatch(p, MS_TOK_SEMICOLON);
  if (p->hadError) {
    msParserSyncError(p);
  }
}

static MsNode* parseShortDecl(MsParser* p, MsNode* target) {
  MsNode* value = msParseExpr(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_SHORT_DECL;
  n->pos = target->pos;
  n->varDecl.name = target->ident.name;
  n->varDecl.nameLen = target->ident.len;
  n->varDecl.init = value;
  return n;
}

// ---------------------------------------------------------------------------
// Block parser — shared by statements that use brace-delimited bodies.
// ---------------------------------------------------------------------------
MsNode* msParseBlock(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  MsNodeList* stmts = NULL;
  MsNodeList** tail = &stmts;

  while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {
  }

  while (!msParserCheck(p, MS_TOK_RBRACE) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* stmt = msParseStmt(p);
    if (stmt != NULL) {
      msNodeListAppend(p, &tail, stmt);
    }
    while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {
    }
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' to close block");

  MsNode* block = MS_ARENA_NEW(p->arena, MsNode);
  block->kind = MS_ND_BLOCK;
  block->pos = pos;
  block->block.stmts = stmts;
  return block;
}

// ---------------------------------------------------------------------------
// If statement parser
// ---------------------------------------------------------------------------
static MsNode* parseIfStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  MsNode* cond = parsePrecedence(p, PREC_OR);

  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after if condition");
  MsNode* thenBlock = msParseBlock(p);

  MsNode* elseBlock = NULL;
  if (msParserMatch(p, MS_TOK_ELSE)) {
    if (msParserMatch(p, MS_TOK_IF)) {
      elseBlock = parseIfStmt(p);
    } else {
      msParserExpect(p, MS_TOK_LBRACE, "expected '{' after 'else'");
      elseBlock = msParseBlock(p);
    }
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_IF;
  n->pos = pos;
  n->ifStmt.cond = cond;
  n->ifStmt.thenBlock = thenBlock;
  n->ifStmt.elseBlock = elseBlock;
  return n;
}

// ---------------------------------------------------------------------------
// For statement parser (T028)
// ---------------------------------------------------------------------------
static MsNode* parseForStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  // 1. Infinite loop: for { }
  if (msParserCheck(p, MS_TOK_LBRACE)) {
    msParserAdvance(p);
    MsNode* body = msParseBlock(p);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_FOR;
    n->pos = pos;
    n->forStmt.init = NULL;
    n->forStmt.cond = NULL;
    n->forStmt.post = NULL;
    n->forStmt.body = body;
    n->forStmt.forTarget = NULL;
    n->forStmt.forIter = NULL;
    return n;
  }

  // 2. for-in: detect ident [, ident]* in pattern
  //    'in' is a Pratt infix op, so msParseExpr would consume it as binary.
  //    Detect for-in before calling msParseExpr by peeking ahead.
  if (msParserCheck(p, MS_TOK_IDENT)) {
    struct MsToken peek = msParserPeekNext(p);
    if (peek.kind == MS_TOK_IN || peek.kind == MS_TOK_COMMA) {
      msParserAdvance(p);
      MsNode* firstId = MS_ARENA_NEW(p->arena, MsNode);
      firstId->kind = MS_ND_IDENT;
      firstId->pos = p->prev.pos;
      firstId->ident.name = p->prev.start;
      firstId->ident.len = p->prev.len;

      MsNode* target = firstId;
      if (msParserMatch(p, MS_TOK_COMMA)) {
        MsNodeList* elems = NULL;
        MsNodeList** tail = &elems;
        msNodeListAppend(p, &tail, firstId);
        do {
          msParserExpect(p, MS_TOK_IDENT, "expected identifier in for-in target");
          MsNode* id = MS_ARENA_NEW(p->arena, MsNode);
          id->kind = MS_ND_IDENT;
          id->pos = p->prev.pos;
          id->ident.name = p->prev.start;
          id->ident.len = p->prev.len;
          msNodeListAppend(p, &tail, id);
        } while (msParserMatch(p, MS_TOK_COMMA));

        target = MS_ARENA_NEW(p->arena, MsNode);
        target->kind = MS_ND_TUPLE;
        target->pos = firstId->pos;
        target->container.elems = elems;
      }

      msParserExpect(p, MS_TOK_IN, "expected 'in' after for-in target");
      MsNode* iter = msParseExpr(p);
      msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for-in iterable");
      MsNode* body = msParseBlock(p);

      MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
      n->kind = MS_ND_FOR;
      n->pos = pos;
      n->forStmt.init = NULL;
      n->forStmt.cond = NULL;
      n->forStmt.post = NULL;
      n->forStmt.body = body;
      n->forStmt.forTarget = target;
      n->forStmt.forIter = iter;
      return n;
    }
  }

  // 3. Condition / three-part: parse first expression (no 'in' ambiguity)
  MsNode* first = msParseExpr(p);

  // Short declaration in init position: i := 0
  if (first != NULL && first->kind == MS_ND_IDENT && msParserMatch(p, MS_TOK_COLON_ASSIGN)) {
    first = parseShortDecl(p, first);
  }

  // 3a. Three-part: for init; cond; post { }
  if (msParserMatch(p, MS_TOK_SEMICOLON)) {
    MsNode* init = first;
    MsNode* cond = NULL;
    if (!msParserCheck(p, MS_TOK_SEMICOLON)) {
      cond = msParseExpr(p);
    }
    msParserExpect(p, MS_TOK_SEMICOLON, "expected ';' after for condition");
    MsNode* post = NULL;
    if (!msParserCheck(p, MS_TOK_LBRACE)) {
      post = msParseExpr(p);
    }
    msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for post");
    MsNode* body = msParseBlock(p);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_FOR;
    n->pos = pos;
    n->forStmt.init = init;
    n->forStmt.cond = cond;
    n->forStmt.post = post;
    n->forStmt.body = body;
    n->forStmt.forTarget = NULL;
    n->forStmt.forIter = NULL;
    return n;
  }

  // 3b. Condition loop: for cond { }
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for condition");
  MsNode* body = msParseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_FOR;
  n->pos = pos;
  n->forStmt.init = NULL;
  n->forStmt.cond = first;
  n->forStmt.post = NULL;
  n->forStmt.body = body;
  n->forStmt.forTarget = NULL;
  n->forStmt.forIter = NULL;
  return n;
}

// ---------------------------------------------------------------------------
// Statement parsing
// ---------------------------------------------------------------------------
MsNode* msParseStmt(MsParser* p) {
  // var declaration
  if (msParserMatch(p, MS_TOK_VAR)) {
    MsNode* n = parseVarDecl(p);
    finishStmt(p);
    return n;
  }

  // if statement
  if (msParserMatch(p, MS_TOK_IF)) {
    return parseIfStmt(p);
  }

  // for statement
  if (msParserMatch(p, MS_TOK_FOR)) {
    return parseForStmt(p);
  }

  MsNode* expr = msParseExpr(p);

  if (expr == NULL) {
    finishStmt(p);
    return NULL;
  }

  MsTokKind tok = p->cur.kind;
  if (tok == MS_TOK_COLON_ASSIGN) {
    // short declaration: x := expr
    if (expr->kind != MS_ND_IDENT) {
      msParserError(p, "expected identifier before ':='");
      finishStmt(p);
      return NULL;
    }
    msParserAdvance(p);
    MsNode* n = parseShortDecl(p, expr);
    finishStmt(p);
    return n;
  }

  if (tok == MS_TOK_ASSIGN) {
    // plain assignment: lvalue = expr
    msParserAdvance(p);
    MsNode* value = msParseExpr(p);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_ASSIGN;
    n->pos = expr->pos;
    n->assign.target = expr;
    n->assign.value = value;
    finishStmt(p);
    return n;
  }

  if (isCompoundAssign(tok)) {
    // compound assignment: lvalue op= expr
    msParserAdvance(p);
    MsNode* value = msParseExpr(p);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_COMPOUND_ASSIGN;
    n->pos = expr->pos;
    n->binary.op = tok;
    n->binary.left = expr;
    n->binary.right = value;
    finishStmt(p);
    return n;
  }

  // expression statement (default)
  finishStmt(p);

  MsNode* stmt = MS_ARENA_NEW(p->arena, MsNode);
  stmt->kind = MS_ND_EXPR_STMT;
  stmt->pos = expr->pos;
  stmt->exprStmt.expr = expr;
  return stmt;
}

MsNode* msParseProgram(MsParser* p) {
  MsNode* prog = MS_ARENA_NEW(p->arena, MsNode);
  prog->kind = MS_ND_PROGRAM;
  prog->pos = p->cur.pos;
  prog->program.filename = p->lex.fileName;
  prog->program.stmts = NULL;

  MsNodeList** tail = &prog->program.stmts;

  while (!msParserCheck(p, MS_TOK_EOF)) {
    if (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {
      continue;
    }
    MsNode* stmt = msParseStmt(p);
    if (stmt == NULL) {
      continue;
    }
    msNodeListAppend(p, &tail, stmt);
  }
  return prog;
}
