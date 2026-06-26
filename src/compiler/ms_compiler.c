#include "mslang/ms_compiler.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ms_scope.h"
#include "mslang/ms_alloc.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_value.h"
#include "parser/ms_arena.h"

// forward declarations
static void compileExpr(MsCompiler* c, MsNode* node);

static void compilerError(MsCompiler* c, struct MsSrcPos pos, const char* fmt, ...) {
  (void)pos;
  if (c->result->hadError) {
    return;
  }
  c->result->hadError = true;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(c->result->errBuf, sizeof(c->result->errBuf), fmt, ap);
  va_end(ap);
}

static void emitConst(MsCompiler* c, MsValue v, uint32_t line) {
  uint32_t idx = msChunkAddConst(c->chunk, v);
  msChunkEmitOpAX(c->chunk, OP_CONST, idx, line);
}

static void compileInt(MsCompiler* c, MsNode* n) {
  emitConst(c, MS_INT_VAL(n->litInt.ival), n->pos.line);
}

static void compileFloat(MsCompiler* c, MsNode* n) {
  emitConst(c, MS_FLOAT_VAL(n->litFloat.fval), n->pos.line);
}

static void compileStringOrBytes(MsCompiler* c, MsNode* n) {
  // T049 前无 msNewStr/msNewBytes，用 MS_NIL_VAL 占位
  emitConst(c, MS_NIL_VAL, n->pos.line);
}

static void compileBool(MsCompiler* c, MsNode* n) {
  MsOpCode op = n->litBool.bval ? OP_CONST_TRUE : OP_CONST_FALSE;
  msChunkEmitOp(c->chunk, op, n->pos.line);
}

static void compileIdent(MsCompiler* c, MsNode* n) {
  // stub: full impl in T040
  compilerError(c, n->pos, "identifier compilation not yet implemented");
}

static void compileUnary(MsCompiler* c, MsNode* n) {
  compileExpr(c, n->unary.operand);
  uint32_t line = n->pos.line;
  switch (n->unary.op) {
    case MS_TOK_MINUS:
      msChunkEmitOp(c->chunk, OP_NEG, line);
      break;
    case MS_TOK_NOT:
      msChunkEmitOp(c->chunk, OP_NOT, line);
      break;
    case MS_TOK_TILDE:
      msChunkEmitOp(c->chunk, OP_BNOT, line);
      break;
    case MS_TOK_PLUS:
      break;
    default:
      compilerError(c, n->pos, "unknown unary op");
      break;
  }
}

// emit a jump instruction with placeholder operand, return patch offset
static uint32_t emitJump(MsCompiler* c, MsOpCode op, uint32_t line) {
  msChunkEmitOpAX(c->chunk, op, 0, line);
  return c->chunk->codeLen - 3;  // offset of the 3-byte operand
}

static void patchJump(MsCompiler* c, uint32_t patchOffset) {
  msChunkPatchJump(c->chunk, patchOffset, c->chunk->codeLen);
}

static void compileBinary(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  if (n->binary.op == MS_TOK_AND) {
    compileExpr(c, n->binary.left);
    uint32_t j = emitJump(c, OP_AND_JMP, line);
    compileExpr(c, n->binary.right);
    patchJump(c, j);
    return;
  }
  if (n->binary.op == MS_TOK_OR) {
    compileExpr(c, n->binary.left);
    uint32_t j = emitJump(c, OP_OR_JMP, line);
    compileExpr(c, n->binary.right);
    patchJump(c, j);
    return;
  }

  compileExpr(c, n->binary.left);
  compileExpr(c, n->binary.right);
  switch (n->binary.op) {
    case MS_TOK_PLUS:
      msChunkEmitOp(c->chunk, OP_ADD, line);
      break;
    case MS_TOK_MINUS:
      msChunkEmitOp(c->chunk, OP_SUB, line);
      break;
    case MS_TOK_STAR:
      msChunkEmitOp(c->chunk, OP_MUL, line);
      break;
    case MS_TOK_SLASH:
      msChunkEmitOp(c->chunk, OP_DIV, line);
      break;
    case MS_TOK_PERCENT:
      msChunkEmitOp(c->chunk, OP_MOD, line);
      break;
    case MS_TOK_STARSTAR:
      msChunkEmitOp(c->chunk, OP_POW, line);
      break;
    case MS_TOK_SHL:
      msChunkEmitOp(c->chunk, OP_SHL, line);
      break;
    case MS_TOK_SHR:
      msChunkEmitOp(c->chunk, OP_SHR, line);
      break;
    case MS_TOK_AMP:
      msChunkEmitOp(c->chunk, OP_BAND, line);
      break;
    case MS_TOK_PIPE:
      msChunkEmitOp(c->chunk, OP_BOR, line);
      break;
    case MS_TOK_CARET:
      msChunkEmitOp(c->chunk, OP_BXOR, line);
      break;
    case MS_TOK_EQ:
      msChunkEmitOp(c->chunk, OP_EQ, line);
      break;
    case MS_TOK_NEQ:
      msChunkEmitOp(c->chunk, OP_NE, line);
      break;
    case MS_TOK_LT:
      msChunkEmitOp(c->chunk, OP_LT, line);
      break;
    case MS_TOK_GT:
      msChunkEmitOp(c->chunk, OP_GT, line);
      break;
    case MS_TOK_LE:
      msChunkEmitOp(c->chunk, OP_LE, line);
      break;
    case MS_TOK_GE:
      msChunkEmitOp(c->chunk, OP_GE, line);
      break;
    case MS_TOK_IS:
      msChunkEmitOp(c->chunk, OP_IS, line);
      break;
    case MS_TOK_IN:
      msChunkEmitOp(c->chunk, OP_IN, line);
      break;
    case MS_TOK_IS_NOT:
      msChunkEmitOp(c->chunk, OP_IS_NOT, line);
      break;
    case MS_TOK_NOT_IN:
      msChunkEmitOp(c->chunk, OP_NOT_IN, line);
      break;
    default:
      compilerError(c, n->pos, "unknown binary op");
      break;
  }
}

static void compileFString(MsCompiler* c, MsNode* n) {
  uint32_t partCount = 0;
  for (MsNodeList* l = n->fstring.parts; l; l = l->next) {
    compileExpr(c, l->node);
    if (l->node->kind != MS_ND_STRING) {
      msChunkEmitOp(c->chunk, OP_TO_STR, n->pos.line);
    }
    partCount++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_STR, (uint8_t) partCount, n->pos.line);
}

static void compileExpr(MsCompiler* c, MsNode* node) {
  if (!node) {
    return;
  }
  switch (node->kind) {
    case MS_ND_INT:
      compileInt(c, node);
      break;
    case MS_ND_FLOAT:
      compileFloat(c, node);
      break;
    case MS_ND_STRING:
    case MS_ND_BYTES:
      compileStringOrBytes(c, node);
      break;
    case MS_ND_BOOL:
      compileBool(c, node);
      break;
    case MS_ND_NIL:
      msChunkEmitOp(c->chunk, OP_CONST_NIL, node->pos.line);
      break;
    case MS_ND_IDENT:
      compileIdent(c, node);
      break;
    case MS_ND_UNARY:
      compileUnary(c, node);
      break;
    case MS_ND_BINARY:
      compileBinary(c, node);
      break;
    case MS_ND_FSTRING:
      compileFString(c, node);
      break;
    default:
      compilerError(c, node->pos, "cannot compile expression kind %d", node->kind);
      break;
  }
}

static void compileStmt(MsCompiler* c, MsNode* node) {
  if (!node) {
    return;
  }
  switch (node->kind) {
    case MS_ND_EXPR_STMT:
      compileExpr(c, node->exprStmt.expr);
      msChunkEmitOp(c->chunk, OP_POP, node->pos.line);
      break;
    default:
      compilerError(c, node->pos, "cannot compile statement kind %d", node->kind);
      break;
  }
}

MsCompileResult msCompile(const char* src, uint32_t srcLen, const char* fileName) {
  MsCompileResult result = {0};

  // parse
  struct MsArena arena;
  msArenaInit(&arena);
  MsParser parser;
  msParserInit(&parser, src, srcLen, fileName, &arena);
  MsNode* program = msParseProgram(&parser);
  if (parser.hadError) {
    result.hadError = true;
    snprintf(result.errBuf, sizeof(result.errBuf), "%s", parser.errBuf);
    msArenaFree(&arena);
    return result;
  }

  // compile
  struct MsChunk* chunk = MS_ALLOC(struct MsChunk);
  msChunkInit(chunk, NULL);
  result.chunk = chunk;

  MsCompiler compiler;
  msCompilerInit(&compiler, NULL, chunk, false);
  compiler.result = &result;

  for (MsNodeList* s = program->program.stmts; s; s = s->next) {
    compileStmt(&compiler, s->node);
    if (result.hadError) {
      break;
    }
  }

  if (!result.hadError) {
    msChunkEmitOp(chunk, OP_RETURN_NIL, 0);
  }

  msCompilerFree(&compiler);
  msArenaFree(&arena);
  return result;
}

void msCompileResultFree(MsCompileResult* r) {
  if (r->chunk) {
    msChunkFree(r->chunk);
    msFree(r->chunk);
    r->chunk = NULL;
  }
}
