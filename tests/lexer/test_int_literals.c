#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

struct IntCase { const char* src; int64_t expected; };

static void testDecimal(void) {
  struct IntCase cases[] = {
    {"0",   0}, {"42", 42}, {"1_000", 1000},
    {"9_223_372_036_854_775_807", INT64_MAX},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind,     MS_TOK_INT,        cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].expected, cases[i].src);
  }
}

static void testHexOctBin(void) {
  struct IntCase cases[] = {
    {"0xFF",   255}, {"0XFF",   255},
    {"0o77",   63},  {"0O77",   63},
    {"0b1010", 10},  {"0B1010", 10},
    {"0x1_0",  16},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind,     MS_TOK_INT,        cases[i].src);
    MS_ASSERT_EQ(t.val.ival, cases[i].expected, cases[i].src);
  }
}

static void testOverflow(void) {
  // 9223372036854775808 = INT64_MAX + 1, bit pattern = INT64_MIN
  const char* src = "9223372036854775808";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_INT, "overflow is MS_TOK_INT");
  MS_ASSERT_EQ(t.val.ival, INT64_MIN, "wraps to INT64_MIN");
}

static void testInvalidLiterals(void) {
  const char* cases[] = {"1_", "1__2", "0777", "0x", "0x_FF", "0b2"};
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i], (uint32_t)strlen(cases[i]), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, cases[i]);
  }
}

int main(void) {
  MS_RUN(testDecimal);
  MS_RUN(testHexOctBin);
  MS_RUN(testOverflow);
  MS_RUN(testInvalidLiterals);
  return msTestSummary();
}
