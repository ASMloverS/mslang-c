// msTokenPrint: serialize a single token to text for the `mslang tokens` command.
#include "mslang/ms_lexer.h"

#include <inttypes.h>
#include <stdio.h>

// Return the KIND string (MS_TOK_ prefix stripped, uppercase) for a token kind.
static const char* tokKindStr(MsTokKind kind) {
  switch (kind) {
    case MS_TOK_INT:               return "INT";
    case MS_TOK_FLOAT:             return "FLOAT";
    case MS_TOK_STRING:            return "STRING";
    case MS_TOK_BYTES:             return "BYTES";
    case MS_TOK_FSTRING_START:     return "FSTRING_START";
    case MS_TOK_FSTRING_PART:      return "FSTRING_PART";
    case MS_TOK_FSTRING_EXPR_START: return "FSTRING_EXPR_START";
    case MS_TOK_FSTRING_EXPR_END:   return "FSTRING_EXPR_END";
    case MS_TOK_FSTRING_END:       return "FSTRING_END";
    case MS_TOK_TRUE:              return "TRUE";
    case MS_TOK_FALSE:             return "FALSE";
    case MS_TOK_NIL:               return "NIL";
    case MS_TOK_IDENT:             return "IDENT";
    case MS_TOK_IF:                return "IF";
    case MS_TOK_ELSE:              return "ELSE";
    case MS_TOK_FOR:               return "FOR";
    case MS_TOK_BREAK:             return "BREAK";
    case MS_TOK_CONTINUE:          return "CONTINUE";
    case MS_TOK_RETURN:            return "RETURN";
    case MS_TOK_FUNC:              return "FUNC";
    case MS_TOK_CLASS:             return "CLASS";
    case MS_TOK_EXTENDS:           return "EXTENDS";
    case MS_TOK_IMPORT:            return "IMPORT";
    case MS_TOK_AS:                return "AS";
    case MS_TOK_VAR:               return "VAR";
    case MS_TOK_AND:               return "AND";
    case MS_TOK_OR:                return "OR";
    case MS_TOK_NOT:               return "NOT";
    case MS_TOK_IN:                return "IN";
    case MS_TOK_IS:                return "IS";
    case MS_TOK_TRY:               return "TRY";
    case MS_TOK_CATCH:             return "CATCH";
    case MS_TOK_FINALLY:           return "FINALLY";
    case MS_TOK_RAISE:             return "RAISE";
    case MS_TOK_GO:                return "GO";
    case MS_TOK_CHAN:               return "CHAN";
    case MS_TOK_SELECT:            return "SELECT";
    case MS_TOK_ASYNC:             return "ASYNC";
    case MS_TOK_AWAIT:             return "AWAIT";
    case MS_TOK_MAKE:              return "MAKE";
    case MS_TOK_PASS:              return "PASS";
    case MS_TOK_SWITCH:            return "SWITCH";
    case MS_TOK_CASE:              return "CASE";
    case MS_TOK_DEFAULT:           return "DEFAULT";
    case MS_TOK_FALLTHROUGH:       return "FALLTHROUGH";
    case MS_TOK_WITH:              return "WITH";
    case MS_TOK_DEL:               return "DEL";
    case MS_TOK_ASSERT:            return "ASSERT";
    case MS_TOK_PLUS:              return "PLUS";
    case MS_TOK_MINUS:             return "MINUS";
    case MS_TOK_STAR:              return "STAR";
    case MS_TOK_SLASH:             return "SLASH";
    case MS_TOK_PERCENT:           return "PERCENT";
    case MS_TOK_STARSTAR:          return "STARSTAR";
    case MS_TOK_AMP:               return "AMP";
    case MS_TOK_PIPE:              return "PIPE";
    case MS_TOK_CARET:             return "CARET";
    case MS_TOK_SHL:               return "SHL";
    case MS_TOK_SHR:               return "SHR";
    case MS_TOK_TILDE:             return "TILDE";
    case MS_TOK_EQ:                return "EQ";
    case MS_TOK_NEQ:               return "NEQ";
    case MS_TOK_LT:                return "LT";
    case MS_TOK_LE:                return "LE";
    case MS_TOK_GT:                return "GT";
    case MS_TOK_GE:                return "GE";
    case MS_TOK_ASSIGN:            return "ASSIGN";
    case MS_TOK_COLON_ASSIGN:      return "COLON_ASSIGN";
    case MS_TOK_PLUS_ASSIGN:       return "PLUS_ASSIGN";
    case MS_TOK_MINUS_ASSIGN:      return "MINUS_ASSIGN";
    case MS_TOK_STAR_ASSIGN:       return "STAR_ASSIGN";
    case MS_TOK_SLASH_ASSIGN:      return "SLASH_ASSIGN";
    case MS_TOK_PERCENT_ASSIGN:    return "PERCENT_ASSIGN";
    case MS_TOK_AMP_ASSIGN:        return "AMP_ASSIGN";
    case MS_TOK_PIPE_ASSIGN:       return "PIPE_ASSIGN";
    case MS_TOK_CARET_ASSIGN:      return "CARET_ASSIGN";
    case MS_TOK_SHL_ASSIGN:        return "SHL_ASSIGN";
    case MS_TOK_SHR_ASSIGN:        return "SHR_ASSIGN";
    case MS_TOK_ARROW_LEFT:        return "ARROW_LEFT";
    case MS_TOK_DOTDOTDOT:         return "DOTDOTDOT";
    case MS_TOK_INC:               return "INC";
    case MS_TOK_DEC:               return "DEC";
    case MS_TOK_DOT:               return "DOT";
    case MS_TOK_COMMA:             return "COMMA";
    case MS_TOK_SEMICOLON:         return "SEMICOLON";
    case MS_TOK_COLON:             return "COLON";
    case MS_TOK_LPAREN:            return "LPAREN";
    case MS_TOK_RPAREN:            return "RPAREN";
    case MS_TOK_LBRACKET:          return "LBRACKET";
    case MS_TOK_RBRACKET:          return "RBRACKET";
    case MS_TOK_LBRACE:            return "LBRACE";
    case MS_TOK_RBRACE:            return "RBRACE";
    case MS_TOK_NEWLINE:           return "NEWLINE";
    case MS_TOK_EOF:               return "EOF";
    case MS_TOK_ERROR:             return "ERROR";
    default:                       return "UNKNOWN";
  }
}

void msTokenPrint(const struct MsToken* tok,
                  const struct MsLexer* lex,
                  FILE* fp) {
  const char* kind = tokKindStr(tok->kind);

  char reprBuf[MS_LEXER_ERR_MAX + 16];
  const char* repr = reprBuf;

  switch (tok->kind) {
    case MS_TOK_INT:
      snprintf(reprBuf, sizeof(reprBuf), "%" PRId64, tok->val.ival);
      break;
    case MS_TOK_FLOAT:
      snprintf(reprBuf, sizeof(reprBuf), "%.17g", tok->val.fval);
      break;
    case MS_TOK_IDENT:
    case MS_TOK_STRING:
    case MS_TOK_BYTES:
    case MS_TOK_FSTRING_START:
    case MS_TOK_FSTRING_PART:
    case MS_TOK_FSTRING_EXPR_START:
    case MS_TOK_FSTRING_EXPR_END:
    case MS_TOK_FSTRING_END:
      fprintf(fp, "%3u:%-4u %-20s %.*s\n",
              tok->pos.line, tok->pos.col, kind,
              (int)tok->len, tok->start);
      return;
    case MS_TOK_NEWLINE:
      repr = "<newline>";
      break;
    case MS_TOK_EOF:
      repr = "<eof>";
      break;
    case MS_TOK_ERROR:
      snprintf(reprBuf, sizeof(reprBuf), "<error: %s>", lex->errBuf);
      break;
    default:
      repr = msTokName(tok->kind);
      break;
  }

  fprintf(fp, "%3u:%-4u %-20s %s\n",
          tok->pos.line, tok->pos.col, kind, repr);
}
