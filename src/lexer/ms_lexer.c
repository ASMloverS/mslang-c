#include "mslang/ms_lexer.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool lexAtEnd(const struct MsLexer* lex) {
  return lex->pos >= lex->srcLen;
}

static uint8_t lexPeekByte(const struct MsLexer* lex) {
  if (lex->pos >= lex->srcLen) {
    return 0;
  }
  return (uint8_t)lex->src[lex->pos];
}

static uint8_t lexPeekByte2(const struct MsLexer* lex) {
  if (lex->pos + 1 >= lex->srcLen) {
    return 0;
  }
  return (uint8_t)lex->src[lex->pos + 1];
}

static uint8_t lexAdvance(struct MsLexer* lex) {
  return (uint8_t)lex->src[lex->pos++];
}

static void lexConsumeNewline(struct MsLexer* lex) {
  // pos already past '\n'; update line tracking
  lex->line++;
  lex->lineStart = lex->pos;
}

static struct MsSrcPos lexCurrentPos(const struct MsLexer* lex) {
  struct MsSrcPos p;
  p.file = lex->fileName;
  p.line = lex->line;
  p.col  = lex->pos - lex->lineStart + 1;
  return p;
}

static struct MsToken lexMakeToken(const struct MsLexer* lex,
                                   MsTokKind kind,
                                   uint32_t start,
                                   struct MsSrcPos pos) {
  struct MsToken t = {kind, pos, lex->src + start, lex->pos - start, {0}};
  return t;
}

static struct MsToken lexMakeEof(const struct MsLexer* lex) {
  return lexMakeToken(lex, MS_TOK_EOF, lex->pos, lexCurrentPos(lex));
}

static struct MsToken lexMakeError(struct MsLexer* lex,
                                   uint32_t start,
                                   struct MsSrcPos pos,
                                   const char* msg) {
  lex->hasError = true;
  snprintf(lex->errBuf, MS_LEXER_ERR_MAX, "%s:%u:%u: %s",
           pos.file, pos.line, pos.col, msg);
  return lexMakeToken(lex, MS_TOK_ERROR, start, pos);
}

// ---------------------------------------------------------------------------
// Single-char and two-char operator scanning
// ---------------------------------------------------------------------------

// Returns true if c2 matches and advances past it, building a 2-char token.
static bool lexMatch(struct MsLexer* lex, uint8_t c2) {
  if (lexPeekByte(lex) == c2) {
    lex->pos++;
    return true;
  }
  return false;
}

// Scan operators/delimiters starting at already-consumed byte `c`.
// pos already points past `c` on entry.
static struct MsToken lexScanOperator(struct MsLexer* lex,
                                      uint8_t c,
                                      uint32_t start,
                                      struct MsSrcPos pos) {
  switch (c) {
    case '+':
      if (lexMatch(lex, '+')) {
        return lexMakeToken(lex, MS_TOK_INC, start, pos);
      }
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_PLUS_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_PLUS, start, pos);
    case '-':
      if (lexMatch(lex, '-')) {
        return lexMakeToken(lex, MS_TOK_DEC, start, pos);
      }
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_MINUS_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_MINUS, start, pos);
    case '*':
      if (lexMatch(lex, '*')) {
        return lexMakeToken(lex, MS_TOK_STARSTAR, start, pos);
      }
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_STAR_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_STAR, start, pos);
    case '/':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_SLASH_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_SLASH, start, pos);
    case '%':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_PERCENT_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_PERCENT, start, pos);
    case '&':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_AMP_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_AMP, start, pos);
    case '|':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_PIPE_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_PIPE, start, pos);
    case '^':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_CARET_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_CARET, start, pos);
    case '~': return lexMakeToken(lex, MS_TOK_TILDE, start, pos);
    case '=':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_EQ, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_ASSIGN, start, pos);
    case '!':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_NEQ, start, pos);
      }
      return lexMakeError(lex, start, pos, "unexpected '!'");
    case '<':
      if (lexMatch(lex, '<')) {
        if (lexMatch(lex, '=')) {
          return lexMakeToken(lex, MS_TOK_SHL_ASSIGN, start, pos);
        }
        return lexMakeToken(lex, MS_TOK_SHL, start, pos);
      }
      if (lexMatch(lex, '-')) {
        return lexMakeToken(lex, MS_TOK_ARROW_LEFT, start, pos);
      }
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_LE, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_LT, start, pos);
    case '>':
      if (lexMatch(lex, '>')) {
        if (lexMatch(lex, '=')) {
          return lexMakeToken(lex, MS_TOK_SHR_ASSIGN, start, pos);
        }
        return lexMakeToken(lex, MS_TOK_SHR, start, pos);
      }
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_GE, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_GT, start, pos);
    case ':':
      if (lexMatch(lex, '=')) {
        return lexMakeToken(lex, MS_TOK_COLON_ASSIGN, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_COLON, start, pos);
    case '.':
      if (lexPeekByte(lex) == '.' && lexPeekByte2(lex) == '.') {
        lex->pos += 2;
        return lexMakeToken(lex, MS_TOK_DOTDOTDOT, start, pos);
      }
      return lexMakeToken(lex, MS_TOK_DOT, start, pos);
    case ',': return lexMakeToken(lex, MS_TOK_COMMA, start, pos);
    case ';': return lexMakeToken(lex, MS_TOK_SEMICOLON, start, pos);
    case '(': return lexMakeToken(lex, MS_TOK_LPAREN, start, pos);
    case ')': return lexMakeToken(lex, MS_TOK_RPAREN, start, pos);
    case '[': return lexMakeToken(lex, MS_TOK_LBRACKET, start, pos);
    case ']': return lexMakeToken(lex, MS_TOK_RBRACKET, start, pos);
    case '{': return lexMakeToken(lex, MS_TOK_LBRACE, start, pos);
    case '}': return lexMakeToken(lex, MS_TOK_RBRACE, start, pos);
    default: {
      char msg[64];
      snprintf(msg, sizeof(msg), "unexpected character '\\x%02x'", (unsigned)c);
      return lexMakeError(lex, start, pos, msg);
    }
  }
}

// ---------------------------------------------------------------------------
// Identifier and keyword scanning
// ---------------------------------------------------------------------------

// syntax.md §1.4 — all 38 keywords, sorted alphabetically for binary search.
static const struct { const char* word; MsTokKind kind; } kKeywords[] = {
  {"and",         MS_TOK_AND},
  {"as",          MS_TOK_AS},
  {"assert",      MS_TOK_ASSERT},
  {"async",       MS_TOK_ASYNC},
  {"await",       MS_TOK_AWAIT},
  {"break",       MS_TOK_BREAK},
  {"case",        MS_TOK_CASE},
  {"catch",       MS_TOK_CATCH},
  {"chan",         MS_TOK_CHAN},
  {"class",       MS_TOK_CLASS},
  {"continue",    MS_TOK_CONTINUE},
  {"default",     MS_TOK_DEFAULT},
  {"del",         MS_TOK_DEL},
  {"else",        MS_TOK_ELSE},
  {"extends",     MS_TOK_EXTENDS},
  {"fallthrough", MS_TOK_FALLTHROUGH},
  {"false",       MS_TOK_FALSE},
  {"finally",     MS_TOK_FINALLY},
  {"for",         MS_TOK_FOR},
  {"func",        MS_TOK_FUNC},
  {"go",          MS_TOK_GO},
  {"if",          MS_TOK_IF},
  {"import",      MS_TOK_IMPORT},
  {"in",          MS_TOK_IN},
  {"is",          MS_TOK_IS},
  {"make",        MS_TOK_MAKE},
  {"nil",         MS_TOK_NIL},
  {"not",         MS_TOK_NOT},
  {"or",          MS_TOK_OR},
  {"pass",        MS_TOK_PASS},
  {"raise",       MS_TOK_RAISE},
  {"return",      MS_TOK_RETURN},
  {"select",      MS_TOK_SELECT},
  {"switch",      MS_TOK_SWITCH},
  {"true",        MS_TOK_TRUE},
  {"try",         MS_TOK_TRY},
  {"var",         MS_TOK_VAR},
  {"with",        MS_TOK_WITH},
};

#define KEYWORD_COUNT (sizeof(kKeywords) / sizeof(kKeywords[0]))
_Static_assert(KEYWORD_COUNT == 38, "syntax.md §1.4 keyword count");

// Binary search in kKeywords[]. Returns keyword kind or MS_TOK_IDENT.
static MsTokKind lexLookupKeyword(const char* start, uint32_t len) {
  int lo = 0;
  int hi = (int)KEYWORD_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int cmp = strncmp(start, kKeywords[mid].word, len);
    if (cmp == 0) {
      // Exact match only if lengths equal.
      cmp = (int)len - (int)strlen(kKeywords[mid].word);
    }
    if (cmp < 0) {
      hi = mid - 1;
    } else if (cmp > 0) {
      lo = mid + 1;
    } else {
      return kKeywords[mid].kind;
    }
  }
  return MS_TOK_IDENT;
}

// Scan an identifier or keyword. Called when the first byte is already confirmed
// to be a valid identifier start (alpha, '_', or >= 0x80).
static struct MsToken lexScanIdent(struct MsLexer* lex,
                                   uint32_t start,
                                   struct MsSrcPos pos) {
  while (!lexAtEnd(lex)) {
    uint8_t c = lexPeekByte(lex);
    if (isalnum((unsigned char)c) || c == '_' || c >= 0x80) {
      lex->pos++;
    } else {
      break;
    }
  }
  uint32_t len = lex->pos - start;
  MsTokKind kind = lexLookupKeyword(lex->src + start, len);
  return lexMakeToken(lex, kind, start, pos);
}

// ---------------------------------------------------------------------------
// msLexerScan: produce one raw token (no peek caching)
// ---------------------------------------------------------------------------

static struct MsToken lexScan(struct MsLexer* lex) {
  // Skip whitespace and line comments (preserve newlines for ASI)
  while (!lexAtEnd(lex)) {
    uint8_t c = lexPeekByte(lex);
    if (c == ' ' || c == '\t' || c == '\r') {
      lex->pos++;
      continue;
    }
    if (c == '/' && lexPeekByte2(lex) == '/') {
      lex->pos += 2;
      while (!lexAtEnd(lex) && lexPeekByte(lex) != '\n') {
        lex->pos++;
      }
      continue;
    }
    break;
  }

  if (lexAtEnd(lex)) {
    return lexMakeEof(lex);
  }

  uint32_t start = lex->pos;
  struct MsSrcPos pos = lexCurrentPos(lex);

  uint8_t c = lexAdvance(lex);

  if (c == '\n') {
    lexConsumeNewline(lex);
    return lexMakeToken(lex, MS_TOK_NEWLINE, start, pos);
  }

  // Identifier or keyword
  if (isalpha((unsigned char)c) || c == '_' || c >= 0x80) {
    return lexScanIdent(lex, start, pos);
  }

  if (c == '$') {
    if (lexPeekByte(lex) == '"') {
      lex->pos++;
      while (!lexAtEnd(lex) && lexPeekByte(lex) != '"') {
        if (lexPeekByte(lex) == '\n') {
          break;
        }
        lex->pos++;
      }
      if (lexAtEnd(lex) || lexPeekByte(lex) != '"') {
        return lexMakeError(lex, start, pos, "unterminated f-string");
      }
      lex->pos++;
      return lexMakeToken(lex, MS_TOK_FSTRING, start, pos);
    }
    return lexMakeError(lex, start, pos, "unexpected '$'");
  }

  return lexScanOperator(lex, c, start, pos);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void msLexerInit(struct MsLexer* lex, const char* src, uint32_t len,
                 const char* fileName) {
  lex->src       = src;
  lex->srcLen    = len;
  lex->pos       = 0;
  lex->line      = 1;
  lex->lineStart = 0;
  lex->fileName  = fileName;
  lex->hasPeek   = false;
  lex->hasError  = false;
  lex->errBuf[0] = '\0';
}

struct MsToken msLexerNext(struct MsLexer* lex) {
  if (lex->hasPeek) {
    lex->hasPeek = false;
    return lex->peek;
  }
  return lexScan(lex);
}

struct MsToken msLexerPeek(struct MsLexer* lex) {
  if (!lex->hasPeek) {
    lex->peek    = lexScan(lex);
    lex->hasPeek = true;
  }
  return lex->peek;
}

struct MsToken msLexerNextSkipNewline(struct MsLexer* lex) {
  struct MsToken t;
  do {
    t = msLexerNext(lex);
  } while (t.kind == MS_TOK_NEWLINE);
  return t;
}

// ---------------------------------------------------------------------------
// msTokName
// ---------------------------------------------------------------------------

const char* msTokName(MsTokKind kind) {
  switch (kind) {
    // Operators / delimiters: return symbol literal
    case MS_TOK_PLUS:          return "+";
    case MS_TOK_MINUS:         return "-";
    case MS_TOK_STAR:          return "*";
    case MS_TOK_SLASH:         return "/";
    case MS_TOK_PERCENT:       return "%";
    case MS_TOK_STARSTAR:      return "**";
    case MS_TOK_AMP:           return "&";
    case MS_TOK_PIPE:          return "|";
    case MS_TOK_CARET:         return "^";
    case MS_TOK_SHL:           return "<<";
    case MS_TOK_SHR:           return ">>";
    case MS_TOK_TILDE:         return "~";
    case MS_TOK_EQ:            return "==";
    case MS_TOK_NEQ:           return "!=";
    case MS_TOK_LT:            return "<";
    case MS_TOK_LE:            return "<=";
    case MS_TOK_GT:            return ">";
    case MS_TOK_GE:            return ">=";
    case MS_TOK_ASSIGN:        return "=";
    case MS_TOK_COLON_ASSIGN:  return ":=";
    case MS_TOK_PLUS_ASSIGN:   return "+=";
    case MS_TOK_MINUS_ASSIGN:  return "-=";
    case MS_TOK_STAR_ASSIGN:   return "*=";
    case MS_TOK_SLASH_ASSIGN:  return "/=";
    case MS_TOK_PERCENT_ASSIGN: return "%=";
    case MS_TOK_AMP_ASSIGN:    return "&=";
    case MS_TOK_PIPE_ASSIGN:   return "|=";
    case MS_TOK_CARET_ASSIGN:  return "^=";
    case MS_TOK_SHL_ASSIGN:    return "<<=";
    case MS_TOK_SHR_ASSIGN:    return ">>=";
    case MS_TOK_ARROW_LEFT:    return "<-";
    case MS_TOK_DOTDOTDOT:     return "...";
    case MS_TOK_INC:           return "++";
    case MS_TOK_DEC:           return "--";
    case MS_TOK_DOT:           return ".";
    case MS_TOK_COMMA:         return ",";
    case MS_TOK_SEMICOLON:     return ";";
    case MS_TOK_COLON:         return ":";
    case MS_TOK_LPAREN:        return "(";
    case MS_TOK_RPAREN:        return ")";
    case MS_TOK_LBRACKET:      return "[";
    case MS_TOK_RBRACKET:      return "]";
    case MS_TOK_LBRACE:        return "{";
    case MS_TOK_RBRACE:        return "}";

    // Keywords: lowercase name
    case MS_TOK_IF:          return "if";
    case MS_TOK_ELSE:        return "else";
    case MS_TOK_FOR:         return "for";
    case MS_TOK_BREAK:       return "break";
    case MS_TOK_CONTINUE:    return "continue";
    case MS_TOK_RETURN:      return "return";
    case MS_TOK_FUNC:        return "func";
    case MS_TOK_CLASS:       return "class";
    case MS_TOK_EXTENDS:     return "extends";
    case MS_TOK_IMPORT:      return "import";
    case MS_TOK_AS:          return "as";
    case MS_TOK_VAR:         return "var";
    case MS_TOK_AND:         return "and";
    case MS_TOK_OR:          return "or";
    case MS_TOK_NOT:         return "not";
    case MS_TOK_IN:          return "in";
    case MS_TOK_IS:          return "is";
    case MS_TOK_TRY:         return "try";
    case MS_TOK_CATCH:       return "catch";
    case MS_TOK_FINALLY:     return "finally";
    case MS_TOK_RAISE:       return "raise";
    case MS_TOK_GO:          return "go";
    case MS_TOK_CHAN:         return "chan";
    case MS_TOK_SELECT:      return "select";
    case MS_TOK_ASYNC:       return "async";
    case MS_TOK_AWAIT:       return "await";
    case MS_TOK_MAKE:        return "make";
    case MS_TOK_PASS:        return "pass";
    case MS_TOK_SWITCH:      return "switch";
    case MS_TOK_CASE:        return "case";
    case MS_TOK_DEFAULT:     return "default";
    case MS_TOK_FALLTHROUGH: return "fallthrough";
    case MS_TOK_WITH:        return "with";
    case MS_TOK_DEL:         return "del";
    case MS_TOK_ASSERT:      return "assert";
    case MS_TOK_TRUE:        return "true";
    case MS_TOK_FALSE:       return "false";
    case MS_TOK_NIL:         return "nil";

    // Others: uppercase kind name
    case MS_TOK_INT:     return "INT";
    case MS_TOK_FLOAT:   return "FLOAT";
    case MS_TOK_STRING:  return "STRING";
    case MS_TOK_FSTRING: return "FSTRING";
    case MS_TOK_BYTES:   return "BYTES";
    case MS_TOK_IDENT:   return "IDENT";
    case MS_TOK_NEWLINE: return "NEWLINE";
    case MS_TOK_EOF:     return "EOF";
    case MS_TOK_ERROR:   return "ERROR";
    default:             return "UNKNOWN";
  }
}
