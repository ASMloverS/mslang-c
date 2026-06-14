#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testEmptySource(void) {
  struct MsLexer lex;
  msLexerInit(&lex, "", 0, "<test>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_EOF, "empty -> EOF");
}

static void testPeekDoesNotConsume(void) {
  struct MsLexer lex;
  const char* src = "+";
  msLexerInit(&lex, src, 1, "<test>");
  struct MsToken p1 = msLexerPeek(&lex);
  struct MsToken p2 = msLexerPeek(&lex);
  struct MsToken n  = msLexerNext(&lex);
  MS_ASSERT_EQ(p1.kind, n.kind, "peek1 == next");
  MS_ASSERT_EQ(p2.kind, n.kind, "peek2 == next");
}

static void testLineTracking(void) {
  // Use an ASI-triggering token (ident) before the newline so NEWLINE is produced.
  struct MsLexer lex;
  const char* src = "x\n+";
  msLexerInit(&lex, src, 3, "<test>");
  struct MsToken t1 = msLexerNext(&lex);
  struct MsToken nl = msLexerNext(&lex);
  struct MsToken t2 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.pos.line, 1, "first token on line 1");
  MS_ASSERT_EQ(nl.kind, MS_TOK_NEWLINE, "newline token");
  MS_ASSERT_EQ(t2.pos.line, 2, "second token on line 2");
}

static void testUnknownCharProducesError(void) {
  struct MsLexer lex;
  const char* src = "@";
  msLexerInit(&lex, src, 1, "<test>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "@ -> MS_TOK_ERROR");
  MS_ASSERT_TRUE(lex.hasError, "hasError == true");
  struct MsToken eof = msLexerNext(&lex);
  MS_ASSERT_EQ(eof.kind, MS_TOK_EOF, "after error -> EOF");
}

static void testTokNameOperator(void) {
  MS_ASSERT_STR_EQ(msTokName(MS_TOK_PLUS), "+", "msTokName(PLUS) == \"+\"");
}

static void testTokNameKeyword(void) {
  MS_ASSERT_STR_EQ(msTokName(MS_TOK_IF), "if", "msTokName(IF) == \"if\"");
}

static void testTokNameEof(void) {
  MS_ASSERT_STR_EQ(msTokName(MS_TOK_EOF), "EOF", "msTokName(EOF) == \"EOF\"");
}

int main(void) {
  MS_RUN(testEmptySource);
  MS_RUN(testPeekDoesNotConsume);
  MS_RUN(testLineTracking);
  MS_RUN(testUnknownCharProducesError);
  MS_RUN(testTokNameOperator);
  MS_RUN(testTokNameKeyword);
  MS_RUN(testTokNameEof);
  return msTestSummary();
}
