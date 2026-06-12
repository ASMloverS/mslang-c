#include <math.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static void testBasicFloat(void) {
  struct { const char* src; double expected; } cases[] = {
    {"3.14",  3.14},  {"1.",   1.0},  {".5",   0.5},
    {"1e10",  1e10},  {"2e+5", 2e5},  {"1.5E-3", 1.5e-3},
    {"0.0",   0.0},   {"1.0",  1.0},
  };
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i].src, (uint32_t)strlen(cases[i].src), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_FLOAT, cases[i].src);
    MS_ASSERT_TRUE(fabs(t.val.fval - cases[i].expected) < 1e-9, cases[i].src);
  }
}

static void testInfFromLargeExponent(void) {
  struct MsLexer lex;
  msLexerInit(&lex, "1e999", 5, "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_FLOAT, "1e999 is float");
  MS_ASSERT_TRUE(isinf(t.val.fval), "1e999 is inf");
}

static void testInvalidFloat(void) {
  const char* cases[] = {"1e", "1e+", "1.5e-", "1_000.0"};
  for (size_t i = 0; i < ARRAY_LEN(cases); i++) {
    struct MsLexer lex;
    msLexerInit(&lex, cases[i], (uint32_t)strlen(cases[i]), "<t>");
    struct MsToken t = msLexerNext(&lex);
    MS_ASSERT_EQ(t.kind, MS_TOK_ERROR, cases[i]);
  }
}

int main(void) {
  MS_RUN(testBasicFloat);
  MS_RUN(testInfFromLargeExponent);
  MS_RUN(testInvalidFloat);
  return msTestSummary();
}
