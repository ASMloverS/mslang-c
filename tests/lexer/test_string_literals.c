#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testBasicString(void) {
  const char* src = "\"hello\"";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_STRING, "basic string kind");
  MS_ASSERT_EQ(t.len, 7u, "basic string len");
}

static void testUnterminatedString(void) {
  const char* src = "\"no close";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "unterminated string is error");
}

static void testUnescape(void) {
  char out[64];
  char err[128];
  uint32_t outLen = 0;

  // "\n" → 0x0A
  int rc = msUnescapeString("\"\\n\"", 4, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape \\n rc");
  MS_ASSERT_EQ(outLen, 1u, "unescape \\n len");
  MS_ASSERT_EQ((unsigned char)out[0], 10u, "unescape \\n value");

  // "\x41" → 0x41 (A)
  outLen = 0;
  rc = msUnescapeString("\"\\x41\"", 6, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape \\x41 rc");
  MS_ASSERT_EQ(outLen, 1u, "unescape \\x41 len");
  MS_ASSERT_EQ((unsigned char)out[0], 65u, "unescape \\x41 value");
}

static void testUnescapeUnicode(void) {
  char out[64];
  char err[128];
  uint32_t outLen = 0;

  // "\u{41}" → U+0041 → 0x41 in UTF-8 (single byte)
  int rc = msUnescapeString("\"\\u{41}\"", 8, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape \\u{41} rc");
  MS_ASSERT_EQ(outLen, 1u, "unescape \\u{41} len");
  MS_ASSERT_EQ((unsigned char)out[0], 0x41u, "unescape \\u{41} value");
}

static void testLiteralNewlineUnterminated(void) {
  // "abc\n  — literal newline before closing quote
  const char src[] = "\"abc\n";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)(sizeof(src) - 1), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "literal newline terminates string");
}

static void testBackslashAtEOF(void) {
  // "abc\  — backslash is the last byte (no escaped char, no close quote)
  const char src[] = "\"abc\\";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)(sizeof(src) - 1), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "backslash at EOF is error");
}

static void testBackslashNewline(void) {
  // "\<newline>  — backslash followed by literal newline must be an error
  const char src[] = "\"abc\\\n\"";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)(sizeof(src) - 1), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "backslash-newline is error");
}

static void testTwoStrings(void) {
  const char* src = "\"a\" \"b\"";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t1 = msLexerNextSkipNewline(&lex);
  struct MsToken t2 = msLexerNextSkipNewline(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_STRING, "first string");
  MS_ASSERT_EQ(t2.kind, MS_TOK_STRING, "second string");
}

static void testInvalidEscape(void) {
  char out[64];
  char err[128];
  uint32_t outLen = 0;
  int rc = msUnescapeString("\"\\q\"", 4, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, -1, "invalid escape rc");
  MS_ASSERT_EQ(strstr(err, "invalid escape") != NULL, 1, "invalid escape errBuf");
}

static void testOutputBufferTooSmall(void) {
  char out[1];
  char err[128];
  uint32_t outLen = 0;
  // "ab" needs 2 bytes but outBuf is only 1
  int rc = msUnescapeString("\"ab\"", 4, out, 1, &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, -1, "too small rc");
  MS_ASSERT_EQ(strstr(err, "output buffer too small") != NULL, 1, "too small errBuf");
}

static void testUnescape4ByteUtf8(void) {
  // "\u{1F600}" — U+1F600 GRINNING FACE
  // raw = "\"\u{1F600}\"" = 11 chars: " \ u { 1 F 6 0 0 } "
  char out[16];
  char err[128];
  uint32_t outLen = 0;
  int rc = msUnescapeString("\"\\u{1F600}\"", 11, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "4-byte utf8 rc");
  MS_ASSERT_EQ(outLen, 4u, "4-byte utf8 len");
  MS_ASSERT_EQ((unsigned char)out[0], 0xF0u, "4-byte utf8 b0");
  MS_ASSERT_EQ((unsigned char)out[1], 0x9Fu, "4-byte utf8 b1");
  MS_ASSERT_EQ((unsigned char)out[2], 0x98u, "4-byte utf8 b2");
  MS_ASSERT_EQ((unsigned char)out[3], 0x80u, "4-byte utf8 b3");
}

static void testSurrogateRejection(void) {
  char out[64];
  char err[128];
  uint32_t outLen = 0;
  int rc = msUnescapeString("\"\\u{D800}\"", 10, out, sizeof(out), &outLen, err, sizeof(err));
  MS_ASSERT_EQ(rc, -1, "surrogate rc");
}

int main(void) {
  MS_RUN(testBasicString);
  MS_RUN(testUnterminatedString);
  MS_RUN(testUnescape);
  MS_RUN(testUnescapeUnicode);
  MS_RUN(testLiteralNewlineUnterminated);
  MS_RUN(testBackslashAtEOF);
  MS_RUN(testBackslashNewline);
  MS_RUN(testTwoStrings);
  MS_RUN(testInvalidEscape);
  MS_RUN(testOutputBufferTooSmall);
  MS_RUN(testUnescape4ByteUtf8);
  MS_RUN(testSurrogateRejection);
  return msTestSummary();
}
