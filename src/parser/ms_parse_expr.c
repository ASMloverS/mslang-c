// ms_parse_expr.c
// T019: prefix/infix parse functions for literals, unary, and binary operators.
// Registers all rules into gParseRules via msParseExprRegisterRules().
#include <string.h>

#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static MsNode* parseUnary(MsParser* p);
static MsNode* parseBinary(MsParser* p, MsNode* left);
static MsNode* parseIsIn(MsParser* p, MsNode* left);
static MsNode* parseIfExpr(MsParser* p, MsNode* value);
static MsNode* parseAttr(MsParser* p, MsNode* left);
static MsNode* parseIndex(MsParser* p, MsNode* obj);
static MsNode* parseCall(MsParser* p, MsNode* callee);
static MsNode* parsePostfix(MsParser* p, MsNode* left);
static MsNode* parseListLit(MsParser* p);
static MsNode* parseMapOrSetLit(MsParser* p);
static MsNode* parseGroupOrTuple(MsParser* p);
static MsNode* parseFuncLit(MsParser* p);
static MsNode* parseAsyncFuncLit(MsParser* p);
static MsNode* parseMake(MsParser* p);
static MsNode* parseRecv(MsParser* p);

// ---------------------------------------------------------------------------
// Literal prefix parsers
// ---------------------------------------------------------------------------
static MsNode* parseIntLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_INT;
  n->pos = p->prev.pos;
  n->litInt.ival = p->prev.val.ival;
  return n;
}

static MsNode* parseFloatLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_FLOAT;
  n->pos = p->prev.pos;
  n->litFloat.fval = p->prev.val.fval;
  return n;
}

static MsNode* parseStringLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_STRING;
  n->pos = p->prev.pos;
  n->litStr.data = p->prev.start;
  n->litStr.len = p->prev.len;
  return n;
}

static MsNode* parseBytesLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_BYTES;
  n->pos = p->prev.pos;
  n->litStr.data = p->prev.start;
  n->litStr.len = p->prev.len;
  return n;
}

static MsNode* parseTrueLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_BOOL;
  n->pos = p->prev.pos;
  n->litBool.bval = true;
  return n;
}

static MsNode* parseFalseLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_BOOL;
  n->pos = p->prev.pos;
  n->litBool.bval = false;
  return n;
}

static MsNode* parseNilLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_NIL;
  n->pos = p->prev.pos;
  return n;
}

static MsNode* parseIdentLit(MsParser* p) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_IDENT;
  n->pos = p->prev.pos;
  n->ident.name = p->prev.start;
  n->ident.len = p->prev.len;
  return n;
}

// ---------------------------------------------------------------------------
// Unary prefix parser  (-x  +x  ~x  not x)
// ---------------------------------------------------------------------------
static MsNode* parseUnary(MsParser* p) {
  MsTokKind op = p->prev.kind;
  struct MsSrcPos pos = p->prev.pos;
  // 'not' is a logical negation with lower precedence than power.
  // '-', '+', '~' must bind tighter than '**' so that -2**2 == -(2**2).
  Precedence prec = (op == MS_TOK_NOT) ? PREC_NOT : PREC_POWER;
  MsNode* operand = parsePrecedence(p, prec);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_UNARY;
  n->pos = pos;
  n->unary.op = op;
  n->unary.operand = operand;
  return n;
}

// ---------------------------------------------------------------------------
// Binary infix parser (arithmetic, bit, comparison, logic)
// ---------------------------------------------------------------------------
static MsNode* parseBinary(MsParser* p, MsNode* left) {
  MsTokKind op = p->prev.kind;
  struct MsSrcPos pos = p->prev.pos;
  Precedence prec = gParseRules[op].prec;
  // '**' is right-associative: recurse at same precedence level.
  bool rightAssoc = (op == MS_TOK_STARSTAR);
  MsNode* right = parsePrecedence(p, rightAssoc ? prec : (Precedence) (prec + 1));
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_BINARY;
  n->pos = pos;
  n->binary.op = op;
  n->binary.left = left;
  n->binary.right = right;
  return n;
}

// ---------------------------------------------------------------------------
// 'is [not]' / '[not] in' / 'in'  infix parser
// ---------------------------------------------------------------------------
static MsNode* parseIsIn(MsParser* p, MsNode* left) {
  MsTokKind op = p->prev.kind;  // MS_TOK_IS, MS_TOK_IN, or MS_TOK_NOT
  struct MsSrcPos pos = p->prev.pos;

  if (op == MS_TOK_IS && msParserMatch(p, MS_TOK_NOT)) {
    op = MS_TOK_IS_NOT;
  } else if (op == MS_TOK_NOT) {
    msParserExpect(p, MS_TOK_IN, "'in' expected after 'not'");
    op = MS_TOK_NOT_IN;
  }
  // Right operand at PREC_COMPARE + 1 (left-associative, no chaining)
  MsNode* right = parsePrecedence(p, (Precedence) (PREC_COMPARE + 1));
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_BINARY;
  n->pos = pos;
  n->binary.op = op;
  n->binary.left = left;
  n->binary.right = right;
  return n;
}

// ---------------------------------------------------------------------------
// T020: if-expression infix parser  (expr if cond else alt)
// ---------------------------------------------------------------------------
static MsNode* parseIfExpr(MsParser* p, MsNode* value) {
  // 'if' already consumed (p->prev.kind == MS_TOK_IF)
  struct MsSrcPos pos = p->prev.pos;

  // cond: full Expr at PREC_IF_EXPR level
  MsNode* cond = parsePrecedence(p, PREC_IF_EXPR);

  msParserExpect(p, MS_TOK_ELSE, "expected 'else' after condition in if-expression");

  // alt: right-associative — recurse at PREC_IF_EXPR so chaining works
  MsNode* alt = parsePrecedence(p, PREC_IF_EXPR);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_IF_EXPR;
  n->pos = pos;
  n->ifExpr.thenExpr = value;
  n->ifExpr.cond = cond;
  n->ifExpr.elseExpr = alt;
  return n;
}

// ---------------------------------------------------------------------------
// T021: attribute access  obj.name
// ---------------------------------------------------------------------------
static MsNode* parseAttr(MsParser* p, MsNode* left) {
  msParserExpect(p, MS_TOK_IDENT, "expected attribute name after '.'");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_ATTR;
  n->pos = p->prev.pos;
  n->attr.obj = left;
  n->attr.name = p->prev.start;
  n->attr.nameLen = p->prev.len;
  return n;
}

// ---------------------------------------------------------------------------
// T021: subscript/slice  obj[key]  obj[lo:hi:step]
// ---------------------------------------------------------------------------
static MsNode* parseIndex(MsParser* p, MsNode* obj) {
  struct MsSrcPos pos = p->prev.pos;  // '['

  MsNode* lo = NULL;
  MsNode* hi = NULL;
  MsNode* step = NULL;

  if (!msParserCheck(p, MS_TOK_COLON) && !msParserCheck(p, MS_TOK_RBRACKET)) {
    lo = msParseExpr(p);
  }

  if (msParserMatch(p, MS_TOK_COLON)) {
    if (!msParserCheck(p, MS_TOK_COLON) && !msParserCheck(p, MS_TOK_RBRACKET)) {
      hi = msParseExpr(p);
    }
    if (msParserMatch(p, MS_TOK_COLON)) {
      if (!msParserCheck(p, MS_TOK_RBRACKET)) {
        step = msParseExpr(p);
      }
    }
    msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after slice");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_SLICE;
    n->pos = pos;
    n->slice.obj = obj;
    n->slice.lo = lo;
    n->slice.hi = hi;
    n->slice.step = step;
    return n;
  }
  if (lo == NULL) {
    msParserError(p, "empty index not allowed");
    return NULL;
  }
  msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after index");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_INDEX;
  n->pos = pos;
  n->index.obj = obj;
  n->index.key = lo;
  return n;
}

// ---------------------------------------------------------------------------
// T021: function call  f(args...)
// ---------------------------------------------------------------------------
static MsNode* parseCall(MsParser* p, MsNode* callee) {
  struct MsSrcPos pos = p->prev.pos;  // '('

  MsNodeList* args = NULL;
  MsNodeList* kwargs = NULL;
  MsNodeList** argTail = &args;
  MsNodeList** kwTail = &kwargs;

  while (!msParserCheck(p, MS_TOK_RPAREN) && !msParserCheck(p, MS_TOK_EOF)) {
    if (msParserMatch(p, MS_TOK_STARSTAR)) {
      MsNode* inner = msParseExpr(p);
      MsNode* wrap = MS_ARENA_NEW(p->arena, MsNode);
      wrap->kind = MS_ND_DOUBLESTAR_EXPR;
      wrap->starExpr.expr = inner;
      msNodeListAppend(p, &kwTail, wrap);
    } else if (msParserMatch(p, MS_TOK_STAR)) {
      MsNode* inner = msParseExpr(p);
      MsNode* wrap = MS_ARENA_NEW(p->arena, MsNode);
      wrap->kind = MS_ND_STAR_EXPR;
      wrap->starExpr.expr = inner;
      msNodeListAppend(p, &argTail, wrap);
    } else {
      if (p->cur.kind == MS_TOK_IDENT && msParserPeekNext(p).kind == MS_TOK_ASSIGN) {
        msParserAdvance(p);
        const char* kname = p->prev.start;
        uint32_t knamelen = p->prev.len;
        msParserAdvance(p);  // consume '='
        MsNode* val = msParseExpr(p);
        MsNode* kw = MS_ARENA_NEW(p->arena, MsNode);
        kw->kind = MS_ND_KWARG_PAIR;
        kw->kwargPair.name = kname;
        kw->kwargPair.nameLen = knamelen;
        kw->kwargPair.value = val;
        msNodeListAppend(p, &kwTail, kw);
      } else {
        msNodeListAppend(p, &argTail, msParseExpr(p));
      }
    }

    if (!msParserMatch(p, MS_TOK_COMMA)) {
      break;
    }
    if (msParserCheck(p, MS_TOK_RPAREN)) {
      break;
    }  // trailing comma
  }
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after arguments");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_CALL;
  n->pos = pos;
  n->call.callee = callee;
  n->call.args = args;
  n->call.kwargs = kwargs;
  return n;
}

// ---------------------------------------------------------------------------
// T021: postfix ++/--
// ---------------------------------------------------------------------------
static MsNode* parsePostfix(MsParser* p, MsNode* left) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_INC_DEC;
  n->pos = p->prev.pos;
  n->incDec.target = left;
  n->incDec.isInc = (p->prev.kind == MS_TOK_INC);
  return n;
}

// ---------------------------------------------------------------------------
// T022: list literal  [elem, ...]
// ---------------------------------------------------------------------------
static MsNode* parseListLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;  // '['
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;

  while (!msParserCheck(p, MS_TOK_RBRACKET) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* elem = msParseExpr(p);
    msNodeListAppend(p, &tail, elem);
    if (!msParserMatch(p, MS_TOK_COMMA)) {
      break;
    }
    if (msParserCheck(p, MS_TOK_RBRACKET)) {
      break;
    }  // trailing comma
  }
  msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after list");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_LIST;
  n->pos = pos;
  n->container.elems = elems;
  return n;
}

// ---------------------------------------------------------------------------
// T022: map/set literal  {k: v, ...}  or  {elem, ...}
// ---------------------------------------------------------------------------
static MsNode* parseMapOrSetLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;  // '{'

  // empty {} → empty map
  if (msParserMatch(p, MS_TOK_RBRACE)) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_MAP;
    n->pos = pos;
    n->map.pairs = NULL;
    return n;
  }

  // parse first element, then peek for ':'
  MsNode* first = msParseExpr(p);

  if (msParserMatch(p, MS_TOK_COLON)) {
    // map mode: {k: v, k2: v2, ...}
    MsNodeList* pairs = NULL;
    MsNodeList** tail = &pairs;

    MsNode* val = msParseExpr(p);
    MsNode* pair = MS_ARENA_NEW(p->arena, MsNode);
    pair->kind = MS_ND_BINARY;
    pair->pos = first->pos;
    pair->binary.op = MS_TOK_COLON;
    pair->binary.left = first;
    pair->binary.right = val;
    msNodeListAppend(p, &tail, pair);

    while (msParserMatch(p, MS_TOK_COMMA) && !msParserCheck(p, MS_TOK_RBRACE)) {
      MsNode* k = msParseExpr(p);
      msParserExpect(p, MS_TOK_COLON, "expected ':' after map key");
      MsNode* v = msParseExpr(p);
      MsNode* pr = MS_ARENA_NEW(p->arena, MsNode);
      pr->kind = MS_ND_BINARY;
      pr->pos = k->pos;
      pr->binary.op = MS_TOK_COLON;
      pr->binary.left = k;
      pr->binary.right = v;
      msNodeListAppend(p, &tail, pr);
    }
    msParserExpect(p, MS_TOK_RBRACE, "expected '}' after map");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_MAP;
    n->pos = pos;
    n->map.pairs = pairs;
    return n;
  }

  // set mode: {a, b, c, ...}
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;
  msNodeListAppend(p, &tail, first);

  while (msParserMatch(p, MS_TOK_COMMA) && !msParserCheck(p, MS_TOK_RBRACE)) {
    MsNode* elem = msParseExpr(p);
    msNodeListAppend(p, &tail, elem);
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' after set");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_SET;
  n->pos = pos;
  n->container.elems = elems;
  return n;
}

// ---------------------------------------------------------------------------
// T023: grouping parentheses / tuple literal  (expr)  (a, b, c)  (a,)
// ---------------------------------------------------------------------------
static MsNode* parseGroupOrTuple(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;  // '('

  // empty tuple: ()
  if (msParserMatch(p, MS_TOK_RPAREN)) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_TUPLE;
    n->pos = pos;
    n->container.elems = NULL;
    return n;
  }

  MsNode* first = parsePrecedence(p, PREC_IF_EXPR);

  if (msParserMatch(p, MS_TOK_COMMA)) {
    // tuple mode: collect remaining elements
    MsNodeList* elems = NULL;
    MsNodeList** tail = &elems;
    msNodeListAppend(p, &tail, first);

    while (!msParserCheck(p, MS_TOK_RPAREN) && !msParserCheck(p, MS_TOK_EOF)) {
      MsNode* elem = parsePrecedence(p, PREC_IF_EXPR);
      msNodeListAppend(p, &tail, elem);
      if (!msParserMatch(p, MS_TOK_COMMA)) {
        break;
      }
    }
    msParserExpect(p, MS_TOK_RPAREN, "expected ')' after tuple");

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_TUPLE;
    n->pos = pos;
    n->container.elems = elems;
    return n;
  }

  // grouping: (expr) with no comma — return expr directly
  msParserExpect(p, MS_TOK_RPAREN, "expected ')'");
  return first;
}

// T023: bare tuple helper — called by statement parsers after parsing first expr.
// If the next token is not a comma, returns first unchanged.
// Otherwise builds MS_ND_TUPLE from first and all subsequent comma-separated exprs.
MsNode* parseMaybeTuple(MsParser* p, MsNode* first) {
  if (!msParserCheck(p, MS_TOK_COMMA)) {
    return first;
  }
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;
  msNodeListAppend(p, &tail, first);

  while (msParserMatch(p, MS_TOK_COMMA)) {
    if (msParserCheck(p, MS_TOK_NEWLINE) || msParserCheck(p, MS_TOK_SEMICOLON) || msParserCheck(p, MS_TOK_EOF)) {
      break;
    }
    MsNode* elem = parsePrecedence(p, PREC_IF_EXPR);
    msNodeListAppend(p, &tail, elem);
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_TUPLE;
  n->pos = first->pos;
  n->container.elems = elems;
  return n;
}

// ---------------------------------------------------------------------------
// Block parser — consumes '{' then delegates to msParseBlock.
// ---------------------------------------------------------------------------
static MsNode* parseBlock(MsParser* p) {
  msParserExpect(p, MS_TOK_LBRACE, "expected '{'");
  return msParseBlock(p);
}

// ---------------------------------------------------------------------------
// T024: parameter list parser (exported for T034 reuse)
// ---------------------------------------------------------------------------
MsNodeList* msParseParamList(MsParser* p) {
  MsNodeList* params = NULL;
  MsNodeList** tail = &params;
  bool sawVararg = false;
  bool sawKwarg = false;
  bool sawDefault = false;

  while (!msParserCheck(p, MS_TOK_RPAREN) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* param = MS_ARENA_NEW(p->arena, MsNode);
    param->kind = MS_ND_PARAM;
    param->pos = p->cur.pos;
    param->param.name = NULL;
    param->param.nameLen = 0;
    param->param.defaultVal = NULL;
    param->param.isVararg = false;
    param->param.isKwarg = false;

    if (msParserMatch(p, MS_TOK_STARSTAR)) {
      if (sawKwarg) {
        msParserError(p, "only one **kwargs allowed");
      }
      msParserExpect(p, MS_TOK_IDENT, "expected parameter name after '**'");
      param->param.name = p->prev.start;
      param->param.nameLen = p->prev.len;
      param->param.isKwarg = true;
      sawKwarg = true;
    } else if (msParserMatch(p, MS_TOK_DOTDOTDOT)) {
      if (sawKwarg) {
        msParserError(p, "...args must appear before **kwargs");
      }
      if (sawVararg) {
        msParserError(p, "only one ...args allowed");
      }
      if (!params) {
        msParserError(p, "variadic parameter requires a leading positional parameter");
      }
      msParserExpect(p, MS_TOK_IDENT, "expected parameter name after '...'");
      param->param.name = p->prev.start;
      param->param.nameLen = p->prev.len;
      param->param.isVararg = true;
      sawVararg = true;
    } else {
      if (sawKwarg) {
        msParserError(p, "positional parameter after **kwargs");
      }
      if (sawVararg) {
        msParserError(p, "positional parameter after ...args");
      }
      msParserExpect(p, MS_TOK_IDENT, "expected parameter name");
      param->param.name = p->prev.start;
      param->param.nameLen = p->prev.len;
      if (msParserMatch(p, MS_TOK_ASSIGN)) {
        param->param.defaultVal = msParseExpr(p);
        sawDefault = true;
      } else if (sawDefault) {
        msParserError(p, "non-default parameter after default parameter");
      }
    }

    msNodeListAppend(p, &tail, param);

    if (!msParserMatch(p, MS_TOK_COMMA)) {
      break;
    }
  }
  return params;
}

// ---------------------------------------------------------------------------
// T024: function literal  func(params) { body }
// ---------------------------------------------------------------------------
static MsNode* parseFuncLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  msParserExpect(p, MS_TOK_LPAREN, "expected '(' after 'func'");
  MsNodeList* params = msParseParamList(p);
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after parameters");
  MsNode* body = parseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_FUNC_DECL;
  n->pos = pos;
  n->funcDecl.name = NULL;
  n->funcDecl.params = params;
  n->funcDecl.body = body;
  n->funcDecl.isAsync = false;
  return n;
}

// ---------------------------------------------------------------------------
// T024: async function literal  async func(params) { body }
// ---------------------------------------------------------------------------
static MsNode* parseAsyncFuncLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_FUNC, "expected 'func' after 'async'");
  MsNode* fn = parseFuncLit(p);
  fn->pos = pos;
  fn->funcDecl.isAsync = true;
  return fn;
}

// ---------------------------------------------------------------------------
// T025: make(chan) / make(chan, cap)
// ---------------------------------------------------------------------------
static MsNode* parseMake(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_LPAREN, "expected '(' after 'make'");
  msParserExpect(p, MS_TOK_CHAN, "expected 'chan' in make expression");

  MsNode* capExpr = NULL;
  if (msParserMatch(p, MS_TOK_COMMA)) {
    capExpr = msParseExpr(p);
  }
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after make arguments");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_MAKE;
  n->pos = pos;
  n->makeExpr.capExpr = capExpr;
  return n;
}

// ---------------------------------------------------------------------------
// T025: <-ch channel receive (prefix)
// ---------------------------------------------------------------------------
static MsNode* parseRecv(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  MsNode* chanExpr = parsePrecedence(p, PREC_UNARY);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_RECV;
  n->pos = pos;
  n->recv.chanExpr = chanExpr;
  return n;
}

// ---------------------------------------------------------------------------
// Rule registration (called once at startup or from test harness)
// ---------------------------------------------------------------------------
void msParseExprRegisterRules(void) {
  // Literals
  parserRegisterRule(MS_TOK_INT, parseIntLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_FLOAT, parseFloatLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_STRING, parseStringLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_BYTES, parseBytesLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_TRUE, parseTrueLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_FALSE, parseFalseLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_NIL, parseNilLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_IDENT, parseIdentLit, NULL, PREC_NONE);

  // Unary prefix  (+ and - also serve as infix with parseBinary)
  parserRegisterRule(MS_TOK_MINUS, parseUnary, parseBinary, PREC_TERM);
  parserRegisterRule(MS_TOK_PLUS, parseUnary, parseBinary, PREC_TERM);
  parserRegisterRule(MS_TOK_TILDE, parseUnary, NULL, PREC_NONE);
  // 'not': prefix = parseUnary, infix = parseIsIn (for "not in")
  parserRegisterRule(MS_TOK_NOT, parseUnary, parseIsIn, PREC_COMPARE);

  // Arithmetic binary
  parserRegisterRule(MS_TOK_STAR, NULL, parseBinary, PREC_FACTOR);
  parserRegisterRule(MS_TOK_SLASH, NULL, parseBinary, PREC_FACTOR);
  parserRegisterRule(MS_TOK_PERCENT, NULL, parseBinary, PREC_FACTOR);
  parserRegisterRule(MS_TOK_STARSTAR, NULL, parseBinary, PREC_POWER);

  // Bitwise binary
  parserRegisterRule(MS_TOK_SHL, NULL, parseBinary, PREC_SHIFT);
  parserRegisterRule(MS_TOK_SHR, NULL, parseBinary, PREC_SHIFT);
  parserRegisterRule(MS_TOK_AMP, NULL, parseBinary, PREC_BITAND);
  parserRegisterRule(MS_TOK_PIPE, NULL, parseBinary, PREC_BITOR);
  parserRegisterRule(MS_TOK_CARET, NULL, parseBinary, PREC_BITXOR);

  // Comparison
  parserRegisterRule(MS_TOK_EQ, NULL, parseBinary, PREC_COMPARE);
  parserRegisterRule(MS_TOK_NEQ, NULL, parseBinary, PREC_COMPARE);
  parserRegisterRule(MS_TOK_LT, NULL, parseBinary, PREC_COMPARE);
  parserRegisterRule(MS_TOK_GT, NULL, parseBinary, PREC_COMPARE);
  parserRegisterRule(MS_TOK_LE, NULL, parseBinary, PREC_COMPARE);
  parserRegisterRule(MS_TOK_GE, NULL, parseBinary, PREC_COMPARE);

  // 'is' and 'in'
  parserRegisterRule(MS_TOK_IS, NULL, parseIsIn, PREC_COMPARE);
  parserRegisterRule(MS_TOK_IN, NULL, parseIsIn, PREC_COMPARE);

  // Logical
  parserRegisterRule(MS_TOK_AND, NULL, parseBinary, PREC_AND);
  parserRegisterRule(MS_TOK_OR, NULL, parseBinary, PREC_OR);

  // T020: if-expression (a if cond else b)
  parserRegisterRule(MS_TOK_IF, NULL, parseIfExpr, PREC_IF_EXPR);

  // T021: postfix operators (PREC_CALL, left-associative)
  parserRegisterRule(MS_TOK_DOT, NULL, parseAttr, PREC_CALL);
  parserRegisterRule(MS_TOK_LBRACKET, parseListLit, parseIndex, PREC_CALL);
  parserRegisterRule(MS_TOK_LPAREN, parseGroupOrTuple, parseCall, PREC_CALL);
  parserRegisterRule(MS_TOK_LBRACE, parseMapOrSetLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_INC, NULL, parsePostfix, PREC_CALL);
  parserRegisterRule(MS_TOK_DEC, NULL, parsePostfix, PREC_CALL);

  // T024: function literal / async function literal
  parserRegisterRule(MS_TOK_FUNC, parseFuncLit, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_ASYNC, parseAsyncFuncLit, NULL, PREC_NONE);

  // T025: make(chan) / <-ch receive
  parserRegisterRule(MS_TOK_MAKE, parseMake, NULL, PREC_NONE);
  parserRegisterRule(MS_TOK_ARROW_LEFT, parseRecv, NULL, PREC_NONE);
}
