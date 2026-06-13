#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

// Helper: initialize lexer from a C string literal.
static void lexInit(struct MsLexer* lex, const char* src) {
  msLexerInit(lex, src, (uint32_t)strlen(src), "<t>");
}

// b"hello" → MS_TOK_BYTES, len=8 (b + 2 quotes + 5 content bytes)
static void testBytesLiteral(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"hello\"");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_BYTES, "bytes token kind");
  MS_ASSERT_EQ(t.len, 8u, "raw len (b + 2 quotes + 5)");
}

// b alone (not followed by ") → MS_TOK_IDENT
static void testBAloneIsIdent(void) {
  struct MsLexer lex;
  lexInit(&lex, "b ");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "b alone is ident");
}

// bytes_var → MS_TOK_IDENT (b-prefix identifier)
static void testBPrefixIdentifier(void) {
  struct MsLexer lex;
  lexInit(&lex, "bytes_var");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "bytes_var is ident");
}

// B"hello" → MS_TOK_IDENT("B") + MS_TOK_STRING (uppercase B is not bytes)
static void testUppercaseBIsNotBytes(void) {
  struct MsLexer lex;
  lexInit(&lex, "B\"hello\"");
  struct MsToken t1 = msLexerNextSkipNewline(&lex);
  struct MsToken t2 = msLexerNextSkipNewline(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_IDENT, "B is ident");
  MS_ASSERT_EQ(t2.kind, MS_TOK_STRING, "following string is string");
}

// b"abc" is unterminated → MS_TOK_ERROR
static void testUnterminatedBytes(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"abc");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "unterminated bytes is error");
}

// b"\x41\x42\x43" → MS_TOK_BYTES
static void testBytesEscapeHex(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"\\x41\\x42\\x43\"");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_BYTES, "hex-escaped bytes kind");
}

// b"\n\t\r" → MS_TOK_BYTES (escape sequences are legal)
static void testBytesEscapeControl(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"\\n\\t\\r\"");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_BYTES, "control escapes in bytes");
}

// b"abc\xFF" → MS_TOK_BYTES (\xFF is a legal escaped byte)
static void testBytesEscapeFF(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"abc\\xFF\"");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_BYTES, "\\xFF is legal in bytes");
}

// Bare non-ASCII byte (> 0x7F) in bytes literal → MS_TOK_ERROR
static void testBytesRawNonAsciiIsError(void) {
  // b"\xC3\xA9" via literal bytes in source (é in UTF-8)
  const unsigned char kRaw[] = {'b', '"', 0xC3, 0xA9, '"'};
  struct MsLexer lex;
  msLexerInit(&lex, (const char*)kRaw, 5, "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, "raw non-ASCII in bytes is error");
}

// ASI: b"hello" at end of line → BYTES then NEWLINE
static void testBytesAsi(void) {
  struct MsLexer lex;
  lexInit(&lex, "b\"hello\"\n");
  struct MsToken t1 = msLexerNext(&lex);
  struct MsToken t2 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_BYTES, "bytes before newline");
  MS_ASSERT_EQ(t2.kind, MS_TOK_NEWLINE, "newline after bytes (ASI)");
}

// msUnescapeBytes: b"\x41\x42\x43" decodes to "ABC"
// Raw token is: b " \ x 4 1 \ x 4 2 \ x 4 3 " = 15 bytes
static void testUnescapeBytes(void) {
  const char* raw = "b\"\\x41\\x42\\x43\"";
  char out[64];
  char err[128];
  uint32_t outLen = 0;
  int rc = msUnescapeBytes(raw, (uint32_t)strlen(raw), out, sizeof(out), &outLen,
                           err, sizeof(err));
  MS_ASSERT_EQ(rc, 0, "unescape hex rc");
  MS_ASSERT_EQ(outLen, 3u, "unescape hex len");
  MS_ASSERT_EQ((unsigned char)out[0], 0x41u, "unescape hex[0]");
  MS_ASSERT_EQ((unsigned char)out[1], 0x42u, "unescape hex[1]");
  MS_ASSERT_EQ((unsigned char)out[2], 0x43u, "unescape hex[2]");
}

int main(void) {
  MS_RUN(testBytesLiteral);
  MS_RUN(testBAloneIsIdent);
  MS_RUN(testBPrefixIdentifier);
  MS_RUN(testUppercaseBIsNotBytes);
  MS_RUN(testUnterminatedBytes);
  MS_RUN(testBytesEscapeHex);
  MS_RUN(testBytesEscapeControl);
  MS_RUN(testBytesEscapeFF);
  MS_RUN(testBytesRawNonAsciiIsError);
  MS_RUN(testBytesAsi);
  MS_RUN(testUnescapeBytes);
  return msTestSummary();
}
