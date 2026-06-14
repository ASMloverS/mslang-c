#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void lexTok(const char* src, MsTokKind expected) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

static void testArithmetic(void) {
  lexTok("+",   MS_TOK_PLUS);
  lexTok("++",  MS_TOK_INC);
  lexTok("+=",  MS_TOK_PLUS_ASSIGN);
  lexTok("-",   MS_TOK_MINUS);
  lexTok("--",  MS_TOK_DEC);
  lexTok("-=",  MS_TOK_MINUS_ASSIGN);
  lexTok("*",   MS_TOK_STAR);
  lexTok("**",  MS_TOK_STARSTAR);
  lexTok("*=",  MS_TOK_STAR_ASSIGN);
  lexTok("/",   MS_TOK_SLASH);
  lexTok("/=",  MS_TOK_SLASH_ASSIGN);
  lexTok("%",   MS_TOK_PERCENT);
  lexTok("%=",  MS_TOK_PERCENT_ASSIGN);
}

static void testBitwise(void) {
  lexTok("~",   MS_TOK_TILDE);
  lexTok("&",   MS_TOK_AMP);
  lexTok("&=",  MS_TOK_AMP_ASSIGN);
  lexTok("|",   MS_TOK_PIPE);
  lexTok("|=",  MS_TOK_PIPE_ASSIGN);
  lexTok("^",   MS_TOK_CARET);
  lexTok("^=",  MS_TOK_CARET_ASSIGN);
  lexTok("<<",  MS_TOK_SHL);
  lexTok("<<=", MS_TOK_SHL_ASSIGN);
  lexTok(">>",  MS_TOK_SHR);
  lexTok(">>=", MS_TOK_SHR_ASSIGN);
}

static void testComparisons(void) {
  lexTok("<-",  MS_TOK_ARROW_LEFT);
  lexTok("<=",  MS_TOK_LE);
  lexTok("<",   MS_TOK_LT);
  lexTok(">=",  MS_TOK_GE);
  lexTok(">",   MS_TOK_GT);
  lexTok("==",  MS_TOK_EQ);
  lexTok("=",   MS_TOK_ASSIGN);
  lexTok("!=",  MS_TOK_NEQ);
}

static void testDoubleDot(void) {
  struct MsLexer lex;
  msLexerInit(&lex, "..", 2, "<t>");
  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_DOT, "..");
  struct MsToken t2 = msLexerNext(&lex);
  MS_ASSERT_EQ(t2.kind, MS_TOK_DOT, "..");
}

static void testMisc(void) {
  lexTok("...", MS_TOK_DOTDOTDOT);
  lexTok(".",   MS_TOK_DOT);
  lexTok(":=",  MS_TOK_COLON_ASSIGN);
  lexTok(":",   MS_TOK_COLON);
  lexTok(",",   MS_TOK_COMMA);
  lexTok(";",   MS_TOK_SEMICOLON);
  lexTok("(",   MS_TOK_LPAREN);
  lexTok(")",   MS_TOK_RPAREN);
  lexTok("[",   MS_TOK_LBRACKET);
  lexTok("]",   MS_TOK_RBRACKET);
  lexTok("{",   MS_TOK_LBRACE);
  lexTok("}",   MS_TOK_RBRACE);
  lexTok("!",   MS_TOK_ERROR);
}

int main(void) {
  MS_RUN(testArithmetic);
  MS_RUN(testBitwise);
  MS_RUN(testComparisons);
  MS_RUN(testDoubleDot);
  MS_RUN(testMisc);
  return msTestSummary();
}
