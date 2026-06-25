// msAstPrint: serialize AST to indented text for the `mslang parse` command.
#include <inttypes.h>
#include <stdio.h>

#include "mslang/ms_parser.h"

// Return the operator symbol string for a token kind used in binary/unary ops.
static const char* opStr(MsTokKind op) {
  switch (op) {
    case MS_TOK_PLUS:
      return "+";
    case MS_TOK_MINUS:
      return "-";
    case MS_TOK_STAR:
      return "*";
    case MS_TOK_SLASH:
      return "/";
    case MS_TOK_PERCENT:
      return "%";
    case MS_TOK_STARSTAR:
      return "**";
    case MS_TOK_AMP:
      return "&";
    case MS_TOK_PIPE:
      return "|";
    case MS_TOK_CARET:
      return "^";
    case MS_TOK_SHL:
      return "<<";
    case MS_TOK_SHR:
      return ">>";
    case MS_TOK_TILDE:
      return "~";
    case MS_TOK_EQ:
      return "==";
    case MS_TOK_NEQ:
      return "!=";
    case MS_TOK_LT:
      return "<";
    case MS_TOK_LE:
      return "<=";
    case MS_TOK_GT:
      return ">";
    case MS_TOK_GE:
      return ">=";
    case MS_TOK_AND:
      return "and";
    case MS_TOK_OR:
      return "or";
    case MS_TOK_NOT:
      return "not";
    case MS_TOK_IN:
      return "in";
    case MS_TOK_IS:
      return "is";
    case MS_TOK_IS_NOT:
      return "is not";
    case MS_TOK_NOT_IN:
      return "not in";
    case MS_TOK_PLUS_ASSIGN:
      return "+=";
    case MS_TOK_MINUS_ASSIGN:
      return "-=";
    case MS_TOK_STAR_ASSIGN:
      return "*=";
    case MS_TOK_SLASH_ASSIGN:
      return "/=";
    case MS_TOK_PERCENT_ASSIGN:
      return "%=";
    case MS_TOK_AMP_ASSIGN:
      return "&=";
    case MS_TOK_PIPE_ASSIGN:
      return "|=";
    case MS_TOK_CARET_ASSIGN:
      return "^=";
    case MS_TOK_SHL_ASSIGN:
      return "<<=";
    case MS_TOK_SHR_ASSIGN:
      return ">>=";
    default:
      return "?";
  }
}

// Compute length of a raw identifier pointer (not NUL-terminated).
static int nameLen(const char* s) {
  int n = 0;
  while (s[n] && ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= '0' && s[n] <= '9') ||
                  s[n] == '_')) {
    n++;
  }
  return n;
}

static void pad(int indent, FILE* fp) {
  for (int i = 0; i < indent; i++) {
    fputc(' ', fp);
    fputc(' ', fp);
  }
}

static void printList(MsNodeList* list, int indent, FILE* fp) {
  for (MsNodeList* cur = list; cur; cur = cur->next) {
    msAstPrint(cur->node, indent, fp);
  }
}

void msAstPrint(MsNode* node, int indent, FILE* fp) {
  if (!node) {
    return;
  }

  pad(indent, fp);

  switch (node->kind) {
    case MS_ND_PROGRAM:
      fprintf(fp, "PROGRAM\n");
      printList(node->program.stmts, indent + 1, fp);
      break;

    case MS_ND_INT:
      fprintf(fp, "INT %" PRId64 "\n", node->litInt.ival);
      break;

    case MS_ND_FLOAT:
      fprintf(fp, "FLOAT %.17g\n", node->litFloat.fval);
      break;

    case MS_ND_STRING:
      fprintf(fp, "STRING %.*s\n", (int) node->litStr.len, node->litStr.data);
      break;

    case MS_ND_BYTES:
      fprintf(fp, "BYTES %.*s\n", (int) node->litStr.len, node->litStr.data);
      break;

    case MS_ND_BOOL:
      fprintf(fp, "BOOL %s\n", node->litBool.bval ? "true" : "false");
      break;

    case MS_ND_NIL:
      fprintf(fp, "NIL\n");
      break;

    case MS_ND_FSTRING:
      fprintf(fp, "FSTRING\n");
      printList(node->fstring.parts, indent + 1, fp);
      break;

    case MS_ND_IDENT:
      fprintf(fp, "IDENT %.*s\n", (int) node->ident.len, node->ident.name);
      break;

    case MS_ND_UNARY:
      fprintf(fp, "UNARY %s\n", opStr(node->unary.op));
      msAstPrint(node->unary.operand, indent + 1, fp);
      break;

    case MS_ND_BINARY:
      fprintf(fp, "BINARY %s\n", opStr(node->binary.op));
      msAstPrint(node->binary.left, indent + 1, fp);
      msAstPrint(node->binary.right, indent + 1, fp);
      break;

    case MS_ND_IF_EXPR:
      fprintf(fp, "IF_EXPR\n");
      msAstPrint(node->ifExpr.thenExpr, indent + 1, fp);
      msAstPrint(node->ifExpr.cond, indent + 1, fp);
      msAstPrint(node->ifExpr.elseExpr, indent + 1, fp);
      break;

    case MS_ND_CALL:
      fprintf(fp, "CALL\n");
      msAstPrint(node->call.callee, indent + 1, fp);
      printList(node->call.args, indent + 1, fp);
      printList(node->call.kwargs, indent + 1, fp);
      break;

    case MS_ND_ATTR:
      fprintf(fp, "ATTR %.*s\n", (int) node->attr.nameLen, node->attr.name);
      msAstPrint(node->attr.obj, indent + 1, fp);
      break;

    case MS_ND_INDEX:
      fprintf(fp, "INDEX\n");
      msAstPrint(node->index.obj, indent + 1, fp);
      msAstPrint(node->index.key, indent + 1, fp);
      break;

    case MS_ND_SLICE:
      fprintf(fp, "SLICE\n");
      msAstPrint(node->slice.obj, indent + 1, fp);
      msAstPrint(node->slice.lo, indent + 1, fp);
      msAstPrint(node->slice.hi, indent + 1, fp);
      msAstPrint(node->slice.step, indent + 1, fp);
      break;

    case MS_ND_STAR_EXPR:
      fprintf(fp, "STAR_EXPR\n");
      msAstPrint(node->starExpr.expr, indent + 1, fp);
      break;

    case MS_ND_DOUBLESTAR_EXPR:
      fprintf(fp, "DOUBLESTAR_EXPR\n");
      msAstPrint(node->starExpr.expr, indent + 1, fp);
      break;

    case MS_ND_LIST:
      fprintf(fp, "LIST\n");
      printList(node->container.elems, indent + 1, fp);
      break;

    case MS_ND_SET:
      fprintf(fp, "SET\n");
      printList(node->container.elems, indent + 1, fp);
      break;

    case MS_ND_TUPLE:
      fprintf(fp, "TUPLE\n");
      printList(node->container.elems, indent + 1, fp);
      break;

    case MS_ND_MAP:
      fprintf(fp, "MAP\n");
      printList(node->map.pairs, indent + 1, fp);
      break;

    case MS_ND_LISTCOMP:
      fprintf(fp, "LISTCOMP\n");
      msAstPrint(node->listComp.expr, indent + 1, fp);
      printList(node->listComp.comprehensions, indent + 1, fp);
      break;

    case MS_ND_MAKE:
      fprintf(fp, "MAKE\n");
      msAstPrint(node->makeExpr.capExpr, indent + 1, fp);
      break;

    case MS_ND_RECV:
      fprintf(fp, "RECV\n");
      msAstPrint(node->recv.chanExpr, indent + 1, fp);
      break;

    case MS_ND_SEND:
      fprintf(fp, "SEND\n");
      msAstPrint(node->send.chanExpr, indent + 1, fp);
      msAstPrint(node->send.val, indent + 1, fp);
      break;

    case MS_ND_VAR_DECL:
      fprintf(fp, "VAR_DECL name=%.*s\n", (int) node->varDecl.nameLen, node->varDecl.name);
      msAstPrint(node->varDecl.init, indent + 1, fp);
      break;

    case MS_ND_SHORT_DECL:
      fprintf(fp, "SHORT_DECL name=%.*s\n", (int) node->varDecl.nameLen, node->varDecl.name);
      msAstPrint(node->varDecl.init, indent + 1, fp);
      break;

    case MS_ND_ASSIGN:
      fprintf(fp, "ASSIGN\n");
      msAstPrint(node->assign.target, indent + 1, fp);
      msAstPrint(node->assign.value, indent + 1, fp);
      break;

    case MS_ND_COMPOUND_ASSIGN:
      fprintf(fp, "COMPOUND_ASSIGN %s\n", opStr(node->binary.op));
      msAstPrint(node->binary.left, indent + 1, fp);
      msAstPrint(node->binary.right, indent + 1, fp);
      break;

    case MS_ND_INC_DEC:
      fprintf(fp, "INC_DEC %s\n", node->incDec.isInc ? "++" : "--");
      msAstPrint(node->incDec.target, indent + 1, fp);
      break;

    case MS_ND_EXPR_STMT:
      fprintf(fp, "EXPR_STMT\n");
      msAstPrint(node->exprStmt.expr, indent + 1, fp);
      break;

    case MS_ND_BLOCK:
      fprintf(fp, "BLOCK\n");
      printList(node->block.stmts, indent + 1, fp);
      break;

    case MS_ND_IF:
      fprintf(fp, "IF\n");
      msAstPrint(node->ifStmt.cond, indent + 1, fp);
      msAstPrint(node->ifStmt.thenBlock, indent + 1, fp);
      msAstPrint(node->ifStmt.elseBlock, indent + 1, fp);
      break;

    case MS_ND_FOR:
      if (node->forStmt.forTarget) {
        fprintf(fp, "FOR in\n");
        msAstPrint(node->forStmt.forTarget, indent + 1, fp);
        msAstPrint(node->forStmt.forIter, indent + 1, fp);
      } else {
        fprintf(fp, "FOR\n");
        msAstPrint(node->forStmt.init, indent + 1, fp);
        msAstPrint(node->forStmt.cond, indent + 1, fp);
        msAstPrint(node->forStmt.post, indent + 1, fp);
      }
      msAstPrint(node->forStmt.body, indent + 1, fp);
      break;

    case MS_ND_SWITCH:
      fprintf(fp, "SWITCH\n");
      msAstPrint(node->switchStmt.expr, indent + 1, fp);
      printList(node->switchStmt.cases, indent + 1, fp);
      break;

    case MS_ND_RETURN:
      fprintf(fp, "RETURN\n");
      msAstPrint(node->singleExpr.expr, indent + 1, fp);
      break;

    case MS_ND_BREAK:
      if (node->jump.label) {
        fprintf(fp, "BREAK %s\n", node->jump.label);
      } else {
        fprintf(fp, "BREAK\n");
      }
      break;

    case MS_ND_CONTINUE:
      if (node->jump.label) {
        fprintf(fp, "CONTINUE %s\n", node->jump.label);
      } else {
        fprintf(fp, "CONTINUE\n");
      }
      break;

    case MS_ND_PASS:
      fprintf(fp, "PASS\n");
      break;

    case MS_ND_FALLTHROUGH:
      fprintf(fp, "FALLTHROUGH\n");
      break;

    case MS_ND_DEL:
      fprintf(fp, "DEL\n");
      msAstPrint(node->singleExpr.expr, indent + 1, fp);
      break;

    case MS_ND_ASSERT:
      fprintf(fp, "ASSERT\n");
      msAstPrint(node->singleExpr.expr, indent + 1, fp);
      msAstPrint(node->singleExpr.expr2, indent + 1, fp);
      break;

    case MS_ND_RAISE:
      fprintf(fp, "RAISE\n");
      msAstPrint(node->singleExpr.expr, indent + 1, fp);
      break;

    case MS_ND_TRY:
      fprintf(fp, "TRY\n");
      msAstPrint(node->tryStmt.body, indent + 1, fp);
      printList(node->tryStmt.handlers, indent + 1, fp);
      msAstPrint(node->tryStmt.finallyBlock, indent + 1, fp);
      break;

    case MS_ND_GO:
      fprintf(fp, "GO\n");
      msAstPrint(node->goStmt.call, indent + 1, fp);
      break;

    case MS_ND_SELECT:
      fprintf(fp, "SELECT\n");
      printList(node->selectStmt.cases, indent + 1, fp);
      break;

    case MS_ND_WITH:
      fprintf(fp, "WITH");
      if (node->withStmt.asName) {
        fprintf(fp, " as %.*s", nameLen(node->withStmt.asName), node->withStmt.asName);
      }
      fprintf(fp, "\n");
      msAstPrint(node->withStmt.expr, indent + 1, fp);
      msAstPrint(node->withStmt.body, indent + 1, fp);
      break;

    case MS_ND_FUNC_DECL:
      if (node->funcDecl.name) {
        fprintf(fp, "FUNC_DECL name=%.*s\n", nameLen(node->funcDecl.name), node->funcDecl.name);
      } else {
        fprintf(fp, "FUNC_DECL\n");
      }
      printList(node->funcDecl.params, indent + 1, fp);
      msAstPrint(node->funcDecl.body, indent + 1, fp);
      break;

    case MS_ND_ASYNC_FUNC:
      if (node->funcDecl.name) {
        fprintf(fp, "ASYNC_FUNC name=%.*s\n", nameLen(node->funcDecl.name), node->funcDecl.name);
      } else {
        fprintf(fp, "ASYNC_FUNC\n");
      }
      printList(node->funcDecl.params, indent + 1, fp);
      msAstPrint(node->funcDecl.body, indent + 1, fp);
      break;

    case MS_ND_CLASS_DECL: {
      int nl = nameLen(node->classDecl.name);
      fprintf(fp, "CLASS_DECL name=%.*s", nl, node->classDecl.name);
      if (node->classDecl.base) {
        fprintf(fp, " base=%.*s", (int) node->classDecl.base->ident.len, node->classDecl.base->ident.name);
      }
      fprintf(fp, "\n");
      printList(node->classDecl.body, indent + 1, fp);
      break;
    }

    case MS_ND_IMPORT: {
      fprintf(fp, "IMPORT ");
      MsNodeList* seg = node->importStmt.path;
      for (; seg; seg = seg->next) {
        fprintf(fp, "%.*s", (int) seg->node->ident.len, seg->node->ident.name);
        if (seg->next) {
          fputc('.', fp);
        }
      }
      if (node->importStmt.asName) {
        fprintf(fp, " as %.*s", nameLen(node->importStmt.asName), node->importStmt.asName);
      }
      fprintf(fp, "\n");
      break;
    }

    case MS_ND_AWAIT:
      fprintf(fp, "AWAIT\n");
      msAstPrint(node->awaitExpr.expr, indent + 1, fp);
      break;

    case MS_ND_PARAM:
      fprintf(fp, "PARAM %.*s", (int) node->param.nameLen, node->param.name);
      if (node->param.isVararg) {
        fprintf(fp, " *");
      }
      if (node->param.isKwarg) {
        fprintf(fp, " **");
      }
      if (node->param.defaultVal) {
        fprintf(fp, " default=");
      }
      fprintf(fp, "\n");
      msAstPrint(node->param.defaultVal, indent + 1, fp);
      break;

    case MS_ND_CATCH_CLAUSE:
      fprintf(fp, "CATCH");
      if (node->catchClause.asName) {
        fprintf(fp, " as %.*s", nameLen(node->catchClause.asName), node->catchClause.asName);
      }
      fprintf(fp, "\n");
      printList(node->catchClause.typeFilter, indent + 1, fp);
      msAstPrint(node->catchClause.body, indent + 1, fp);
      break;

    case MS_ND_SWITCH_CASE:
      if (node->switchCase.isDefault) {
        fprintf(fp, "SWITCH_CASE default\n");
      } else {
        fprintf(fp, "SWITCH_CASE\n");
        printList(node->switchCase.values, indent + 1, fp);
      }
      msAstPrint(node->switchCase.body, indent + 1, fp);
      break;

    case MS_ND_SELECT_CASE:
      fprintf(fp, "SELECT_CASE\n");
      msAstPrint(node->selectCase.comm, indent + 1, fp);
      msAstPrint(node->selectCase.body, indent + 1, fp);
      break;

    case MS_ND_KWARG_PAIR:
      fprintf(fp, "KWARG %.*s\n", (int) node->kwargPair.nameLen, node->kwargPair.name);
      msAstPrint(node->kwargPair.value, indent + 1, fp);
      break;

    default:
      fprintf(fp, "UNKNOWN(%d)\n", node->kind);
      break;
  }
}
