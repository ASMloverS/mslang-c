// Tests for msTokenPrint output format.
// On Windows fmemopen is unavailable; we write to a temp file instead.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

// Read temp file into buf (up to bufSize-1 bytes), NUL-terminate, return length.
static size_t readTmp(FILE* fp, char* buf, size_t bufSize) {
  rewind(fp);
  size_t n = fread(buf, 1, bufSize - 1, fp);
  buf[n] = '\0';
  return n;
}

static void testIdentRepr(void) {
  const char* src = "hello";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_IDENT, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  // Line 1, col 1, kind "IDENT" padded to 20, repr "hello"
  MS_ASSERT_STR_EQ(buf, "  1:1    IDENT                hello\n", "ident repr");
  // "  1:1    IDENT                hello\n" breakdown:
  // %3u=`  1`, `:`  %-4u=`1   `, ` `, %-20s=`IDENT               `, ` `, hello
}

static void testIntRepr(void) {
  const char* src = "42";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_INT, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  MS_ASSERT_STR_EQ(buf, "  1:1    INT                  42\n", "int repr");
}

static void testKeywordRepr(void) {
  const char* src = "return";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_RETURN, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  MS_ASSERT_STR_EQ(buf, "  1:1    RETURN               return\n", "keyword repr");
}

static void testEofRepr(void) {
  const char* src = "";
  struct MsLexer lex;
  msLexerInit(&lex, src, 0, "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_EOF, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  MS_ASSERT_STR_EQ(buf, "  1:1    EOF                  <eof>\n", "eof repr");
}

static void testNewlineRepr(void) {
  const char* src = "x\ny";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  msLexerNext(&lex); // x
  struct MsToken tok = msLexerNext(&lex); // NEWLINE
  MS_ASSERT_EQ(tok.kind, MS_TOK_NEWLINE, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  MS_ASSERT_STR_EQ(buf, "  1:2    NEWLINE              <newline>\n", "newline repr");
}

static void testErrorRepr(void) {
  const char* src = "@";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_ERROR, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[256];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  // errBuf contains only the message; repr is "<error: msg>"
  MS_ASSERT_TRUE(strstr(buf, "ERROR") != NULL, "has ERROR kind");
  MS_ASSERT_TRUE(strstr(buf, "<error:") != NULL, "has <error: prefix");
}

static void testPlusRepr(void) {
  const char* src = "+";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken tok = msLexerNext(&lex);
  MS_ASSERT_EQ(tok.kind, MS_TOK_PLUS, "kind");

  FILE* fp = tmpfile();
  MS_ASSERT_TRUE(fp != NULL, "tmpfile");
  msTokenPrint(&tok, &lex, fp);

  char buf[128];
  readTmp(fp, buf, sizeof(buf));
  fclose(fp);
  MS_ASSERT_STR_EQ(buf, "  1:1    PLUS                 +\n", "plus repr");
}

int main(void) {
  MS_RUN(testIdentRepr);
  MS_RUN(testIntRepr);
  MS_RUN(testKeywordRepr);
  MS_RUN(testEofRepr);
  MS_RUN(testNewlineRepr);
  MS_RUN(testErrorRepr);
  MS_RUN(testPlusRepr);
  return msTestSummary();
}
