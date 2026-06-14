#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

// Collect token kinds into out[] until MS_TOK_EOF (inclusive) or maxOut reached.
static void collectKinds(const char* src, MsTokKind* out, int maxOut) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  for (int i = 0; i < maxOut; i++) {
    struct MsToken t = msLexerNext(&lex);
    out[i] = t.kind;
    if (t.kind == MS_TOK_EOF) {
      break;
    }
  }
}

static void testSimpleASI(void) {
  MsTokKind kinds[8];
  collectKinds("x\ny", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,   "y");
  MS_ASSERT_EQ(kinds[3], MS_TOK_EOF,     "eof");
}

static void testReturnASI(void) {
  MsTokKind kinds[8];
  collectKinds("return\nx", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_RETURN,  "return");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI after return");
}

static void testNoASIInsideParen(void) {
  // '(' not in ASI list — newline after '(' is ignored.
  // 'x' is in ASI list — newline after 'x' triggers ASI.
  MsTokKind kinds[8];
  collectKinds("(\nx\n)", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_LPAREN,  "lparen");
  MS_ASSERT_EQ(kinds[1], MS_TOK_IDENT,   "x (no ASI before x)");
  MS_ASSERT_EQ(kinds[2], MS_TOK_NEWLINE, "ASI after x");
  MS_ASSERT_EQ(kinds[3], MS_TOK_RPAREN,  "rparen");
}

static void testMultipleNewlines(void) {
  // Only one NEWLINE should be produced even with multiple blank lines.
  MsTokKind kinds[8];
  collectKinds("x\n\n\ny", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "single ASI");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,   "y");
  MS_ASSERT_EQ(kinds[3], MS_TOK_EOF,     "eof");
}

static void testTrailingNewline(void) {
  MsTokKind kinds[8];
  // No trailing newline — no NEWLINE token before EOF.
  collectKinds("x", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT, "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_EOF,   "eof directly");
  // Trailing newline — ASI triggers, then EOF.
  collectKinds("x\n", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI from trailing newline");
  MS_ASSERT_EQ(kinds[2], MS_TOK_EOF,     "eof after ASI");
}

static void testCommentBeforeNewlineASI(void) {
  // "x // comment\ny": comment is skipped, then \n seen; prev=x => ASI.
  MsTokKind kinds[8];
  collectKinds("x // comment\ny", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI after comment newline");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,   "y");
}

static void testNoASIAfterOperator(void) {
  // '+' not in ASI list — newline is ignored.
  MsTokKind kinds[8];
  collectKinds("+\nx", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_PLUS,  "plus");
  MS_ASSERT_EQ(kinds[1], MS_TOK_IDENT, "x (no NEWLINE)");
  MS_ASSERT_EQ(kinds[2], MS_TOK_EOF,   "eof");
}

static void testRBraceASI(void) {
  MsTokKind kinds[8];
  collectKinds("}\nx", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_RBRACE,  "rbrace");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI after }");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,   "x");
}

static void testDotDotDotASI(void) {
  MsTokKind kinds[8];
  collectKinds("...\nx", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_DOTDOTDOT, "...");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE,   "ASI after ...");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,     "x");
}

int main(void) {
  MS_RUN(testSimpleASI);
  MS_RUN(testReturnASI);
  MS_RUN(testNoASIInsideParen);
  MS_RUN(testMultipleNewlines);
  MS_RUN(testTrailingNewline);
  MS_RUN(testCommentBeforeNewlineASI);
  MS_RUN(testNoASIAfterOperator);
  MS_RUN(testRBraceASI);
  MS_RUN(testDotDotDotASI);
  return msTestSummary();
}
