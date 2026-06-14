#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void testCommentSkip(void) {
  const char* src = "// comment\nx";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t;
  do { t = msLexerNext(&lex); } while (t.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "ident after comment");
  MS_ASSERT_EQ(t.pos.line, 2, "on line 2");
}

static void testWhitespaceSkip(void) {
  const char* src = "   \t  x";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "ident after whitespace");
}

static void testCommentOnlyNoNewline(void) {
  const char* src = "// comment";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_EOF, "EOF for comment-only input");
}

static void testMultiLineComment(void) {
  const char* src = "a\n// c1\n// c2\nb";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_IDENT, "a");
  MS_ASSERT_EQ(t1.pos.line, 1, "line 1");
  struct MsToken t2;
  do { t2 = msLexerNext(&lex); } while (t2.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t2.kind, MS_TOK_IDENT, "b");
  MS_ASSERT_EQ(t2.pos.line, 4, "line 4");
}

static void testCommentLineNumber(void) {
  // x // comment\ny: x is line 1, y is line 2 (skip NEWLINE between)
  const char* src = "x // comment\ny";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_IDENT, "x kind");
  MS_ASSERT_EQ(t1.pos.line, 1, "x line 1");
  struct MsToken t2;
  do { t2 = msLexerNext(&lex); } while (t2.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t2.kind, MS_TOK_IDENT, "y kind");
  MS_ASSERT_EQ(t2.pos.line, 2, "y line 2");
}

static void testCrLfSingleIncrement(void) {
  // "\r\n" should only increment line once; next token is on line 2
  const char* src = "\r\nx";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t;
  do { t = msLexerNext(&lex); } while (t.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "x kind after CRLF");
  MS_ASSERT_EQ(t.pos.line, 2, "x on line 2 after CRLF");
}

static void testBlockCommentNotRecognized(void) {
  // "/* block */" is not a block comment: lexed as SLASH, STAR, ident, STAR, SLASH
  const char* src = "/* block */";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_SLASH, "slash from /*");
  struct MsToken t2 = msLexerNext(&lex);
  MS_ASSERT_EQ(t2.kind, MS_TOK_STAR, "star from /*");
  // No error on lexer from block comment syntax
  MS_ASSERT_EQ(lex.hasError, 0, "no lexer error for block comment");
}

int main(void) {
  MS_RUN(testCommentSkip);
  MS_RUN(testWhitespaceSkip);
  MS_RUN(testCommentOnlyNoNewline);
  MS_RUN(testMultiLineComment);
  MS_RUN(testCommentLineNumber);
  MS_RUN(testCrLfSingleIncrement);
  MS_RUN(testBlockCommentNotRecognized);
  return msTestSummary();
}
