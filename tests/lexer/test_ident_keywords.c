#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

// Mirrors kKeywords[] in ms_lexer.c (syntax.md §1.4, 38 keywords, alphabetical).
static const struct { const char* word; MsTokKind kind; } kKeywordCases[] = {
  {"and", MS_TOK_AND},     {"as", MS_TOK_AS},       {"assert", MS_TOK_ASSERT},
  {"async", MS_TOK_ASYNC}, {"await", MS_TOK_AWAIT}, {"break", MS_TOK_BREAK},
  {"case", MS_TOK_CASE},   {"catch", MS_TOK_CATCH}, {"chan", MS_TOK_CHAN},
  {"class", MS_TOK_CLASS}, {"continue", MS_TOK_CONTINUE},
  {"default", MS_TOK_DEFAULT},         {"del", MS_TOK_DEL},
  {"else", MS_TOK_ELSE},   {"extends", MS_TOK_EXTENDS},
  {"fallthrough", MS_TOK_FALLTHROUGH}, {"false", MS_TOK_FALSE},
  {"finally", MS_TOK_FINALLY},         {"for", MS_TOK_FOR},
  {"func", MS_TOK_FUNC},   {"go", MS_TOK_GO},       {"if", MS_TOK_IF},
  {"import", MS_TOK_IMPORT}, {"in", MS_TOK_IN},     {"is", MS_TOK_IS},
  {"make", MS_TOK_MAKE},   {"nil", MS_TOK_NIL},     {"not", MS_TOK_NOT},
  {"or", MS_TOK_OR},       {"pass", MS_TOK_PASS},   {"raise", MS_TOK_RAISE},
  {"return", MS_TOK_RETURN}, {"select", MS_TOK_SELECT},
  {"switch", MS_TOK_SWITCH}, {"true", MS_TOK_TRUE}, {"try", MS_TOK_TRY},
  {"var", MS_TOK_VAR},     {"with", MS_TOK_WITH},
};

#define KEYWORD_CASE_COUNT (sizeof(kKeywordCases) / sizeof(kKeywordCases[0]))
_Static_assert(KEYWORD_CASE_COUNT == 38, "keyword coverage must stay 38/38");

static void lexOne(const char* src, MsTokKind expected) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<test>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

// 38/38 full coverage via table-driven test.
static void testKeywords(void) {
  for (size_t i = 0; i < KEYWORD_CASE_COUNT; i++) {
    lexOne(kKeywordCases[i].word, kKeywordCases[i].kind);
  }
}

// Binary search requires the table to be alphabetically sorted.
static void testKeywordTableSorted(void) {
  for (size_t i = 1; i < KEYWORD_CASE_COUNT; i++) {
    MS_ASSERT_EQ(strcmp(kKeywordCases[i - 1].word, kKeywordCases[i].word) < 0,
                 true, kKeywordCases[i].word);
  }
}

static void testNonKeywordIdents(void) {
  lexOne("len",      MS_TOK_IDENT);
  lexOne("type",     MS_TOK_IDENT);
  lexOne("self",     MS_TOK_IDENT);
  lexOne("x",        MS_TOK_IDENT);
  lexOne("_priv",    MS_TOK_IDENT);
  lexOne("__init__", MS_TOK_IDENT);
}

static void testIdentPrefixOfKeyword(void) {
  lexOne("ifx",     MS_TOK_IDENT);
  lexOne("fore",    MS_TOK_IDENT);
  lexOne("trueish", MS_TOK_IDENT);
}

// Unicode bytes (>= 0x80) are accepted transparently.
static void testUnicodeIdent(void) {
  // UTF-8 encoding of "名前" (Japanese, 6 bytes)
  const char* src = "\xe5\x90\x8d\xe5\x89\x8d";
  lexOne(src, MS_TOK_IDENT);
}

int main(void) {
  MS_RUN(testKeywords);
  MS_RUN(testKeywordTableSorted);
  MS_RUN(testNonKeywordIdents);
  MS_RUN(testIdentPrefixOfKeyword);
  MS_RUN(testUnicodeIdent);
  return msTestSummary();
}
