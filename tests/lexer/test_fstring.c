#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

// Helper: initialize lexer from a C string literal.
static void lexInit(struct MsLexer* lex, const char* src) {
  msLexerInit(lex, src, (uint32_t)strlen(src), "<t>");
}

// $"hello" → FSTRING_START("hello"), FSTRING_END
static void testSimpleFString(void) {
  const char* src = "$\"hello\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_FSTRING_START, "simple: start kind");

  struct MsToken t2 = msLexerNext(&lex);
  MS_ASSERT_EQ(t2.kind, MS_TOK_FSTRING_END, "simple: end kind");
}

// $"hello {name}!" → START, EXPR_START, IDENT, EXPR_END, PART, END
static void testFStringWithExpr(void) {
  const char* src = "$\"hi {x}!\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t;
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_START, "expr: start");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_EXPR_START, "expr: expr_start");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "expr: ident x");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_EXPR_END, "expr: expr_end");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_PART, "expr: part '!'");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_END, "expr: end");
}

// $"{1 + 1}" → START(""), EXPR_START, INT(1), PLUS, INT(1), EXPR_END, PART(""), END
static void testFStringExprOnly(void) {
  const char* src = "$\"{1 + 1}\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t;
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_START, "expr_only: start");
  // start should cover empty fragment — len = 2 ($")
  MS_ASSERT_EQ(t.len, 2u, "expr_only: start len is $\" prefix only");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_EXPR_START, "expr_only: expr_start");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_INT, "expr_only: int 1");
  MS_ASSERT_EQ(t.val.ival, 1, "expr_only: int 1 val");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_PLUS, "expr_only: plus");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_INT, "expr_only: int 1b");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_EXPR_END, "expr_only: expr_end");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_PART, "expr_only: part");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_END, "expr_only: end");
}

// $"{{literal braces}}" — {{ and }} are escape sequences
// Expected: START (covering raw source including {{ and }}), END
static void testFStringLiteralBraces(void) {
  const char* src = "$\"{{literal}}\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t;
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_START, "braces: start kind");
  // start/len covers raw source from $ through last }} (excluding closing ")
  // $"{{literal}} = 13 bytes (positions 0..12)
  MS_ASSERT_EQ(t.len, 13u, "braces: start len covers raw source");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_END, "braces: end kind");
}

// $"未终止 — unterminated f-string (no closing ")
static void testUnterminatedFString(void) {
  const char* src = "$\"no close";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "unterminated: error kind");
}

// $" {x — { without matching } inside f-string
static void testUnmatchedBrace(void) {
  const char* src = "$\" {x";
  struct MsLexer lex;
  lexInit(&lex, src);

  // We should get START, EXPR_START, IDENT(x), then EOF → causes ERROR
  // The simplest check: at some point we get MS_TOK_ERROR or the EXPR_END is
  // never produced before EOF / error.
  // Drain tokens until ERROR or EOF.
  bool gotError = false;
  for (int i = 0; i < 20; i++) {
    struct MsToken t = msLexerNext(&lex);
    if (t.kind == MS_TOK_ERROR) { gotError = true; break; }
    if (t.kind == MS_TOK_EOF)   { break; }
  }
  MS_ASSERT_TRUE(gotError, "unmatched {: produces error");
}

// $"a}b" — single } in OUTER state is illegal
static void testStrayCloseBrace(void) {
  const char* src = "$\"a}b\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  bool gotError = false;
  for (int i = 0; i < 10; i++) {
    struct MsToken t = msLexerNext(&lex);
    if (t.kind == MS_TOK_ERROR) { gotError = true; break; }
    if (t.kind == MS_TOK_EOF)   { break; }
  }
  MS_ASSERT_TRUE(gotError, "stray }: produces error");
}

// $"{ {1: 2}[k] }" — map literal inside expr: depth tracking
// We only verify: START, EXPR_START, (several inner tokens), EXPR_END, PART, END
static void testFStringMapInsideExpr(void) {
  const char* src = "$\"{ {1: 2}[k] }\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t;
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_START, "map: start");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_EXPR_START, "map: expr_start");

  // Drain tokens until EXPR_END or error/EOF
  bool gotExprEnd = false;
  for (int i = 0; i < 30; i++) {
    t = msLexerNext(&lex);
    if (t.kind == MS_TOK_FSTRING_EXPR_END) { gotExprEnd = true; break; }
    if (t.kind == MS_TOK_ERROR || t.kind == MS_TOK_EOF) { break; }
  }
  MS_ASSERT_TRUE(gotExprEnd, "map: expr_end produced correctly");

  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_PART, "map: part after expr");
  t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FSTRING_END, "map: end");
}

// $ not followed by " → MS_TOK_ERROR
static void testBareDollar(void) {
  const char* src = "$x";
  struct MsLexer lex;
  lexInit(&lex, src);

  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "bare $: error");
}

// Nested f-string ($"outer {$"inner"} !") — initial version: MS_TOK_ERROR
static void testNestedFStringForbidden(void) {
  const char* src = "$\"outer {$\"inner\"} !\"";
  struct MsLexer lex;
  lexInit(&lex, src);

  bool gotError = false;
  for (int i = 0; i < 20; i++) {
    struct MsToken t = msLexerNext(&lex);
    if (t.kind == MS_TOK_ERROR) { gotError = true; break; }
    if (t.kind == MS_TOK_EOF)   { break; }
  }
  MS_ASSERT_TRUE(gotError, "nested f-string: error");
}

int main(void) {
  MS_RUN(testSimpleFString);
  MS_RUN(testFStringWithExpr);
  MS_RUN(testFStringExprOnly);
  MS_RUN(testFStringLiteralBraces);
  MS_RUN(testUnterminatedFString);
  MS_RUN(testUnmatchedBrace);
  MS_RUN(testStrayCloseBrace);
  MS_RUN(testFStringMapInsideExpr);
  MS_RUN(testBareDollar);
  MS_RUN(testNestedFStringForbidden);
  return msTestSummary();
}
