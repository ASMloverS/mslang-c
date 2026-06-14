#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Lexer error message buffer capacity (bytes, including NUL).
#define MS_LEXER_ERR_MAX 256

typedef enum MsTokKind {
  // Literals
  MS_TOK_INT,
  MS_TOK_FLOAT,
  MS_TOK_STRING,
  MS_TOK_BYTES,

  // F-string tokens ($"..." interpolation)
  MS_TOK_FSTRING_START,      // $" ... first fragment (raw, up to first { or ")
  MS_TOK_FSTRING_PART,       // fragment between } and next { or "
  MS_TOK_FSTRING_EXPR_START, // { position (enter embedded expression)
  MS_TOK_FSTRING_EXPR_END,   // } position (leave embedded expression)
  MS_TOK_FSTRING_END,        // closing "
  MS_TOK_TRUE,
  MS_TOK_FALSE,
  MS_TOK_NIL,

  // Identifier
  MS_TOK_IDENT,

  // Keywords (true/false/nil are in the literals group above)
  MS_TOK_IF,         MS_TOK_ELSE,        MS_TOK_FOR,
  MS_TOK_BREAK,      MS_TOK_CONTINUE,    MS_TOK_RETURN,
  MS_TOK_FUNC,       MS_TOK_CLASS,       MS_TOK_EXTENDS,
  MS_TOK_IMPORT,     MS_TOK_AS,          MS_TOK_VAR,
  MS_TOK_AND,        MS_TOK_OR,          MS_TOK_NOT,
  MS_TOK_IN,         MS_TOK_IS,
  MS_TOK_TRY,        MS_TOK_CATCH,       MS_TOK_FINALLY,
  MS_TOK_RAISE,      MS_TOK_GO,          MS_TOK_CHAN,
  MS_TOK_SELECT,     MS_TOK_ASYNC,       MS_TOK_AWAIT,
  MS_TOK_MAKE,       MS_TOK_PASS,        MS_TOK_SWITCH,
  MS_TOK_CASE,       MS_TOK_DEFAULT,     MS_TOK_FALLTHROUGH,
  MS_TOK_WITH,       MS_TOK_DEL,         MS_TOK_ASSERT,

  // Operators
  MS_TOK_PLUS,        MS_TOK_MINUS,       MS_TOK_STAR,
  MS_TOK_SLASH,       MS_TOK_PERCENT,
  MS_TOK_STARSTAR,    // ** (power / kwarg spread — parser disambiguates)
  MS_TOK_AMP,         MS_TOK_PIPE,        MS_TOK_CARET,
  MS_TOK_SHL,         MS_TOK_SHR,         MS_TOK_TILDE,
  MS_TOK_EQ,          MS_TOK_NEQ,
  MS_TOK_LT,          MS_TOK_LE,          MS_TOK_GT,    MS_TOK_GE,
  MS_TOK_ASSIGN,      MS_TOK_COLON_ASSIGN,
  MS_TOK_PLUS_ASSIGN,  MS_TOK_MINUS_ASSIGN, MS_TOK_STAR_ASSIGN,
  MS_TOK_SLASH_ASSIGN, MS_TOK_PERCENT_ASSIGN,
  MS_TOK_AMP_ASSIGN,   MS_TOK_PIPE_ASSIGN,  MS_TOK_CARET_ASSIGN,
  MS_TOK_SHL_ASSIGN,   MS_TOK_SHR_ASSIGN,
  MS_TOK_ARROW_LEFT,   // <-
  MS_TOK_DOTDOTDOT,    // ...
  MS_TOK_INC,          MS_TOK_DEC,          // ++ --

  // Delimiters
  MS_TOK_DOT,      MS_TOK_COMMA,     MS_TOK_SEMICOLON,  MS_TOK_COLON,
  MS_TOK_LPAREN,   MS_TOK_RPAREN,
  MS_TOK_LBRACKET, MS_TOK_RBRACKET,
  MS_TOK_LBRACE,   MS_TOK_RBRACE,

  // Special
  MS_TOK_NEWLINE,  // virtual `;` inserted by ASI
  MS_TOK_EOF,
  MS_TOK_ERROR,    // lexer error (message in lex->errBuf)

  MS_TOK_COUNT,
} MsTokKind;

struct MsSrcPos {
  const char* file;   // filename; caller-owned, valid for lexer lifetime
  uint32_t    line;   // 1-based
  uint32_t    col;    // 1-based (byte offset, not Unicode column)
};

struct MsToken {
  MsTokKind       kind;
  struct MsSrcPos pos;    // start position of this token
  const char*     start;  // pointer into src (not a copy)
  uint32_t        len;    // byte length
  union {
    int64_t ival;  // MS_TOK_INT
    double  fval;  // MS_TOK_FLOAT
    // MS_TOK_STRING/BYTES: start/len cover raw token (incl. quotes)
    // MS_TOK_FSTRING_START/PART: start/len cover raw source fragment
  } val;
};

// F-string scanning state.
typedef enum MsFStrState {
  MS_FSTR_NONE,   // not inside an f-string
  MS_FSTR_OUTER,  // inside $"…", scanning string fragments
  MS_FSTR_INNER,  // inside { … } embedded expression
} MsFStrState;

struct MsLexer {
  const char* src;        // full source (UTF-8, immutable)
  uint32_t    srcLen;
  uint32_t    pos;        // current read byte offset
  uint32_t    line;       // current line number
  uint32_t    lineStart;  // byte offset of current line start
  const char* fileName;   // filename (for diagnostics)

  struct MsToken peek;    // peek-ahead token cache
  bool           hasPeek;
  MsTokKind      prevKind; // last non-NEWLINE token kind; used by ASI

  // F-string state machine.
  MsFStrState fstrState;
  int         fstrDepth; // brace nesting depth while in MS_FSTR_INNER

  // Error collection: lexer errors do not abort; parser decides recovery.
  // errBuf is valid only for the most recent MS_TOK_ERROR token.
  char errBuf[MS_LEXER_ERR_MAX];
  bool hasError;
};

// Initialize lexer. src must remain valid for the lexer's lifetime.
void msLexerInit(struct MsLexer* lex, const char* src, uint32_t len,
                 const char* fileName);

// Return next token; call repeatedly until MS_TOK_EOF.
struct MsToken msLexerNext(struct MsLexer* lex);

// Return current peek token without advancing.
struct MsToken msLexerPeek(struct MsLexer* lex);

// Like msLexerNext but skips MS_TOK_NEWLINE tokens.
struct MsToken msLexerNextSkipNewline(struct MsLexer* lex);

// Token kind name for error messages and diagnostics.
// Operators/delimiters: symbol literal (e.g. "+", "(").
// Keywords: lowercase name (e.g. "if").
// Others: uppercase kind name (e.g. "INT", "EOF").
const char* msTokName(MsTokKind kind);

// Decode a string literal token (raw bytes including surrounding quotes).
// Returns 0 on success (*outLen = decoded byte count).
// Returns -1 on error (errBuf filled with message).
// outBuf capacity must be >= rawLen (guaranteed sufficient).
int msUnescapeString(const char* raw, uint32_t rawLen,
                     char* outBuf, uint32_t outBufLen, uint32_t* outLen,
                     char* errBuf, uint32_t errBufLen);

// Decode a bytes literal token (raw bytes including b-prefix and surrounding quotes).
// Same escape rules as msUnescapeString. raw[0]=='b', raw[1]=='"', raw[rawLen-1]=='"'.
// Returns 0 on success (*outLen = decoded byte count).
// Returns -1 on error (errBuf filled with message).
int msUnescapeBytes(const char* raw, uint32_t rawLen,
                    char* outBuf, uint32_t outBufLen, uint32_t* outLen,
                    char* errBuf, uint32_t errBufLen);

// Print one token to fp in the canonical text format:
//   "<line>:<col>  <KIND padded to 20>  <repr>\n"
// lex is read-only (needed for errBuf on MS_TOK_ERROR).
// src is the original source buffer (for IDENT/STRING raw text).
void msTokenPrint(const struct MsToken* tok,
                  const struct MsLexer* lex,
                  FILE* fp);
