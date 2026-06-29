#include "mslang/ms_compiler.h"

#include <assert.h>
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
static void compileStmt(MsCompiler* c, MsNode* node);
static void compileBind(MsCompiler* c, MsNode* target, uint32_t line);
static void compileBlock(MsCompiler* c, MsNode* n);
static void compileCallEx(MsCompiler* c, MsNode* n, uint32_t line);
static void compileCallKw(MsCompiler* c, MsNode* n, uint32_t line);

#define MS_MAX_LOOP_PATCHES 256

typedef struct MsLoopCtx {
  uint32_t breakPatches[MS_MAX_LOOP_PATCHES];
  int breakCount;
  uint32_t continuePatches[MS_MAX_LOOP_PATCHES];
  int continueCount;
  uint32_t loopStart;
  struct MsLoopCtx* outer;
} MsLoopCtx;

static void compilerError(MsCompiler* c, struct MsSrcPos pos, const char* fmt, ...) {
  (void) pos;
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

// T049: replace with real MsStr
static uint32_t addStringConst(MsCompiler* c, const char* name, uint32_t len) {
  (void) name;
  (void) len;
  return msChunkAddConst(c->chunk, MS_NIL_VAL);
}

typedef enum { VAR_LOCAL, VAR_UPVALUE, VAR_GLOBAL } VarScope;

typedef struct {
  VarScope scope;
  uint32_t idx;
} VarLoc;

static VarLoc resolveVar(MsCompiler* c, const char* name, uint32_t len) {
  int local = msScopeResolveLocal(c, name, len);
  if (local >= 0) {
    return (VarLoc){VAR_LOCAL, (uint32_t) local};
  }
  int upval = msScopeResolveUpvalue(c, name, len);
  if (upval >= 0) {
    return (VarLoc){VAR_UPVALUE, (uint32_t) upval};
  }
  uint32_t nameIdx = addStringConst(c, name, len);
  return (VarLoc){VAR_GLOBAL, nameIdx};
}

static void emitGetVar(MsCompiler* c, VarLoc loc, uint32_t line) {
  switch (loc.scope) {
    case VAR_LOCAL:
      msChunkEmitOpA(c->chunk, OP_GET_LOCAL, (uint8_t) loc.idx, line);
      break;
    case VAR_UPVALUE:
      msChunkEmitOpA(c->chunk, OP_GET_UPVALUE, (uint8_t) loc.idx, line);
      break;
    case VAR_GLOBAL:
      msChunkEmitOpAX(c->chunk, OP_GET_GLOBAL, loc.idx, line);
      break;
  }
}

static void emitSetVar(MsCompiler* c, VarLoc loc, uint32_t line) {
  switch (loc.scope) {
    case VAR_LOCAL:
      msChunkEmitOpA(c->chunk, OP_SET_LOCAL, (uint8_t) loc.idx, line);
      break;
    case VAR_UPVALUE:
      msChunkEmitOpA(c->chunk, OP_SET_UPVALUE, (uint8_t) loc.idx, line);
      break;
    case VAR_GLOBAL:
      msChunkEmitOpAX(c->chunk, OP_SET_GLOBAL, loc.idx, line);
      break;
  }
}

static void compileIdent(MsCompiler* c, MsNode* n) {
  VarLoc loc = resolveVar(c, n->ident.name, n->ident.len);
  emitGetVar(c, loc, n->pos.line);
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

static void patchJumpTo(MsCompiler* c, uint32_t patchOffset, uint32_t target) {
  msChunkPatchJump(c->chunk, patchOffset, target);
}

static void emitBackJump(MsCompiler* c, uint32_t loopStart, uint32_t line) {
  uint32_t patch = emitJump(c, OP_JUMP, line);
  patchJumpTo(c, patch, loopStart);
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

static void compileContainerElems(MsCompiler* c, MsNode* n, MsOpCode buildOp) {
  uint32_t line = n->pos.line;
  int count = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) {
    compileExpr(c, l->node);
    count++;
  }
  if (count > UINT8_MAX) {
    compilerError(c, n->pos, "too many elements in literal (max 255)");
    return;
  }
  msChunkEmitOpA(c->chunk, buildOp, (uint8_t) count, line);
}

static void compileList(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  int count = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) {
    MsNode* elem = l->node;
    if (elem->kind == MS_ND_STAR_EXPR) {
      compilerError(c, elem->pos, "list unpacking in literal not supported in v0.1");
      return;
    }
    compileExpr(c, elem);
    count++;
  }
  if (count > UINT8_MAX) {
    compilerError(c, n->pos, "too many elements in literal (max 255)");
    return;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t) count, line);
}

static void compileMap(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  int count = 0;
  for (MsNodeList* l = n->map.pairs; l; l = l->next) {
    MsNode* pair = l->node;
    if (pair->kind == MS_ND_DOUBLESTAR_EXPR) {
      compilerError(c, pair->pos, "dict unpacking in literal not supported in v0.1");
      return;
    }
    compileExpr(c, pair->binary.left);
    compileExpr(c, pair->binary.right);
    count++;
  }
  if (count > UINT8_MAX) {
    compilerError(c, n->pos, "too many pairs in map literal (max 255)");
    return;
  }
  // A = pair count; VM pops 2*A values (key, val alternating)
  msChunkEmitOpA(c->chunk, OP_BUILD_MAP, (uint8_t) count, line);
}

static void compileSliceExpr(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  uint8_t flags = 0;
  compileExpr(c, n->slice.obj);
  if (n->slice.lo) {
    compileExpr(c, n->slice.lo);
    flags |= MS_SLICE_HAS_LO;
  }
  if (n->slice.hi) {
    compileExpr(c, n->slice.hi);
    flags |= MS_SLICE_HAS_HI;
  }
  if (n->slice.step) {
    compileExpr(c, n->slice.step);
    flags |= MS_SLICE_HAS_STEP;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_SLICE, flags, line);
  msChunkEmitOp(c->chunk, OP_GET_ITEM, line);
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

static int identLen(const char* s) {
  int n = 0;
  while (s[n] && ((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= '0' && s[n] <= '9') ||
                  s[n] == '_')) {
    n++;
  }
  return n;
}

// Declare+bind a name as local or global. Returns false on error.
static bool bindDeclName(MsCompiler* c, const char* name, uint32_t nameLen, struct MsSrcPos pos, uint32_t line) {
  VarLoc loc;
  if (c->isFunction) {
    int slot = msScopeDeclareLocal(c, name, nameLen);
    if (slot < 0) {
      compilerError(c, pos, "too many locals or duplicate declaration");
      return false;
    }
    msScopeMarkInitialized(c);
    loc = (VarLoc){VAR_LOCAL, (uint32_t) slot};
  } else {
    uint32_t nameIdx = addStringConst(c, name, nameLen);
    loc = (VarLoc){VAR_GLOBAL, nameIdx};
  }
  emitSetVar(c, loc, line);
  msChunkEmitOp(c->chunk, OP_POP, line);
  return true;
}

// Compile a function node into a sub-chunk and add proto to outer constant
// pool. Returns the constant pool index. Does NOT emit any opcode.
static uint32_t compileFuncProto(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  struct MsChunk* funcChunk = MS_ALLOC(struct MsChunk);
  msChunkInit(funcChunk, NULL);
  MsCompiler funcC;
  msCompilerInit(&funcC, c, funcChunk, true);
  funcC.result = c->result;

  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) {
    MsNode* p = l->node;
    if (p->kind == MS_ND_PARAM) {
      int slot = msScopeDeclareLocal(&funcC, p->param.name, p->param.nameLen);
      if (slot < 0) {
        compilerError(c, p->pos, "too many parameters or duplicate parameter name");
        msCompilerFree(&funcC);
        return 0;
      }
      msScopeMarkInitialized(&funcC);
    }
  }

  compileBlock(&funcC, n->funcDecl.body);
  msChunkEmitOp(funcChunk, OP_RETURN_NIL, line);

  uint32_t funcIdx = msChunkAddConst(c->chunk, MS_NIL_VAL);

  msCompilerFree(&funcC);
  return funcIdx;
}

// Compile a function node: add proto to constant pool, emit OP_MAKE_FUNC +
// upvalue descriptors. Does NOT bind name to scope.
static void compileFuncToConst(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  struct MsChunk* funcChunk = MS_ALLOC(struct MsChunk);
  msChunkInit(funcChunk, NULL);
  MsCompiler funcC;
  msCompilerInit(&funcC, c, funcChunk, true);
  funcC.result = c->result;

  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) {
    MsNode* p = l->node;
    if (p->kind == MS_ND_PARAM) {
      int slot = msScopeDeclareLocal(&funcC, p->param.name, p->param.nameLen);
      if (slot < 0) {
        compilerError(c, p->pos, "too many parameters or duplicate parameter name");
        msCompilerFree(&funcC);
        return;
      }
      msScopeMarkInitialized(&funcC);
    }
  }

  compileBlock(&funcC, n->funcDecl.body);
  msChunkEmitOp(funcChunk, OP_RETURN_NIL, line);

  uint32_t funcIdx = msChunkAddConst(c->chunk, MS_NIL_VAL);
  msChunkEmitOpAX(c->chunk, OP_MAKE_FUNC, funcIdx, line);
  msChunkEmit(c->chunk, (uint8_t) funcC.upvalueCount, line);
  for (int i = 0; i < funcC.upvalueCount; i++) {
    msChunkEmit(c->chunk, funcC.upvalues[i].isLocal ? 1 : 0, line);
    msChunkEmit(c->chunk, funcC.upvalues[i].index, line);
  }

  msCompilerFree(&funcC);
}

static void compileFuncDecl(MsCompiler* c, MsNode* n) {
  compileFuncToConst(c, n);
  if (n->funcDecl.name) {
    int nameL = identLen(n->funcDecl.name);
    bindDeclName(c, n->funcDecl.name, (uint32_t) nameL, n->pos, n->pos.line);
  }
}

static void compileClassDecl(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  struct MsChunk* ck = c->chunk;

  int baseCount = 0;
  if (n->classDecl.base != NULL) {
    compileExpr(c, n->classDecl.base);
    baseCount = 1;
  }

  // collect per-method (mNameIdx, mFuncIdx) pairs
  uint32_t mNameIdxs[256];
  uint32_t mFuncIdxs[256];
  int methodCount = 0;
  for (MsNodeList* l = n->classDecl.body; l; l = l->next) {
    MsNode* member = l->node;
    if (member->kind == MS_ND_FUNC_DECL || member->kind == MS_ND_ASYNC_FUNC) {
      if (methodCount >= 255) {
        compilerError(c, member->pos, "too many methods (max 255)");
        return;
      }
      uint32_t protoIdx = compileFuncProto(c, member);
      int nameL = identLen(member->funcDecl.name);
      uint32_t nameIdx = addStringConst(c, member->funcDecl.name, (uint32_t) nameL);
      mNameIdxs[methodCount] = nameIdx;
      mFuncIdxs[methodCount] = protoIdx;
      methodCount++;
    } else {
      compilerError(c, member->pos, "class attributes not supported yet");
      return;
    }
  }

  // class name constant
  uint32_t classNameIdx = 0;
  if (n->classDecl.name) {
    int nameL = identLen(n->classDecl.name);
    classNameIdx = addStringConst(c, n->classDecl.name, (uint32_t) nameL);
  }

  // emit: OP_MAKE_CLASS [nameIdx:u16 BE] [methodCount:u8] [baseCount:u8]
  msChunkEmitOp(ck, OP_MAKE_CLASS, line);
  msChunkEmit(ck, (uint8_t) ((classNameIdx >> 8) & 0xFF), line);
  msChunkEmit(ck, (uint8_t) (classNameIdx & 0xFF), line);
  msChunkEmit(ck, (uint8_t) methodCount, line);
  msChunkEmit(ck, (uint8_t) baseCount, line);

  // per-method operand pairs: [mNameIdx:u16 BE] [mFuncIdx:u16 BE]
  for (int i = 0; i < methodCount; i++) {
    msChunkEmit(ck, (uint8_t) ((mNameIdxs[i] >> 8) & 0xFF), line);
    msChunkEmit(ck, (uint8_t) (mNameIdxs[i] & 0xFF), line);
    msChunkEmit(ck, (uint8_t) ((mFuncIdxs[i] >> 8) & 0xFF), line);
    msChunkEmit(ck, (uint8_t) (mFuncIdxs[i] & 0xFF), line);
  }

  if (n->classDecl.name) {
    int nameL = identLen(n->classDecl.name);
    bindDeclName(c, n->classDecl.name, (uint32_t) nameL, n->pos, line);
  }
}

static void compileReturn(MsCompiler* c, MsNode* n) {
  if (!c->isFunction) {
    compilerError(c, n->pos, "return outside function");
    return;
  }
  if (n->singleExpr.expr) {
    compileExpr(c, n->singleExpr.expr);
  } else {
    msChunkEmitOp(c->chunk, OP_CONST_NIL, n->pos.line);
  }
  msChunkEmitOp(c->chunk, OP_RETURN, n->pos.line);
}

static void compileCallKw(MsCompiler* c, MsNode* n, uint32_t line) {
  int posArgc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    compileExpr(c, l->node);
    posArgc++;
  }

  int kwCount = 0;
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind == MS_ND_KWARG_PAIR) {
      uint32_t nameIdx = addStringConst(c, kw->kwargPair.name, kw->kwargPair.nameLen);
      msChunkEmitOpAX(c->chunk, OP_CONST, nameIdx, line);
      compileExpr(c, kw->kwargPair.value);
      kwCount++;
    }
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_MAP, (uint8_t) kwCount, line);
  msChunkEmitOpA(c->chunk, OP_CALL_KW, (uint8_t) posArgc, line);
}

static void compileCallEx(MsCompiler* c, MsNode* n, uint32_t line) {
  int plainArgc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    MsNode* a = l->node;
    if (a->kind == MS_ND_STAR_EXPR) {
      if (plainArgc > 0) {
        msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t) plainArgc, line);
        plainArgc = 0;
      }
      compileExpr(c, a->starExpr.expr);
    } else {
      compileExpr(c, a);
      plainArgc++;
    }
  }
  if (plainArgc > 0) {
    msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t) plainArgc, line);
  }

  // TODO T068: handle MS_ND_DOUBLESTAR_EXPR kwargs
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind == MS_ND_DOUBLESTAR_EXPR) {
      compileExpr(c, kw->starExpr.expr);
    }
  }

  msChunkEmitOp(c->chunk, OP_CALL_EX, line);
}

static void compileCall(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->call.callee);

  bool hasStar = false;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    if (l->node->kind == MS_ND_STAR_EXPR) {
      hasStar = true;
      break;
    }
  }

  bool hasDoublestar = false;
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    if (l->node->kind == MS_ND_DOUBLESTAR_EXPR) {
      hasDoublestar = true;
      break;
    }
  }

  if (hasStar || hasDoublestar) {
    compileCallEx(c, n, line);
    return;
  }
  if (n->call.kwargs != NULL) {
    compileCallKw(c, n, line);
    return;
  }

  int argc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    compileExpr(c, l->node);
    argc++;
  }
  msChunkEmitOpA(c->chunk, OP_CALL, (uint8_t) argc, line);
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
    case MS_ND_LIST:
      compileList(c, node);
      break;
    case MS_ND_MAP:
      compileMap(c, node);
      break;
    case MS_ND_SET:
      compileContainerElems(c, node, OP_BUILD_SET);
      break;
    case MS_ND_TUPLE:
      compileContainerElems(c, node, OP_BUILD_TUPLE);
      break;
    case MS_ND_INDEX:
      compileExpr(c, node->index.obj);
      compileExpr(c, node->index.key);
      msChunkEmitOp(c->chunk, OP_GET_ITEM, node->pos.line);
      break;
    case MS_ND_SLICE:
      compileSliceExpr(c, node);
      break;
    case MS_ND_ATTR: {
      compileExpr(c, node->attr.obj);
      uint32_t nameIdx = addStringConst(c, node->attr.name, node->attr.nameLen);
      msChunkEmitOpAX(c->chunk, OP_GET_ATTR, nameIdx, node->pos.line);
      break;
    }
    case MS_ND_FUNC_DECL:
    case MS_ND_ASYNC_FUNC:
      compileFuncDecl(c, node);
      break;
    case MS_ND_CALL:
      compileCall(c, node);
      break;
    case MS_ND_AWAIT:
      compileExpr(c, node->awaitExpr.expr);
      msChunkEmitOp(c->chunk, OP_AWAIT, node->pos.line);
      break;
    default:
      compilerError(c, node->pos, "cannot compile expression kind %d", node->kind);
      break;
  }
}

static void compileVarDecl(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  if (n->varDecl.init) {
    compileExpr(c, n->varDecl.init);
  } else {
    msChunkEmitOp(c->chunk, OP_CONST_NIL, line);
  }

  if (c->isFunction) {
    int slot = msScopeDeclareLocal(c, n->varDecl.name, n->varDecl.nameLen);
    if (slot < 0) {
      compilerError(c, n->pos, "too many local variables or duplicate declaration");
      return;
    }
    msScopeMarkInitialized(c);
  } else {
    uint32_t nameIdx = addStringConst(c, n->varDecl.name, n->varDecl.nameLen);
    msChunkEmitOpAX(c->chunk, OP_SET_GLOBAL, nameIdx, line);
    msChunkEmitOp(c->chunk, OP_POP, line);
  }
}

static void compileAssign(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  MsNode* target = n->assign.target;
  compileExpr(c, n->assign.value);

  switch (target->kind) {
    case MS_ND_IDENT: {
      VarLoc loc = resolveVar(c, target->ident.name, target->ident.len);
      emitSetVar(c, loc, line);
      break;
    }
    case MS_ND_ATTR: {
      compileExpr(c, target->attr.obj);
      uint32_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
      msChunkEmitOpAX(c->chunk, OP_SET_ATTR, nameIdx, line);
      break;
    }
    case MS_ND_INDEX:
      compileExpr(c, target->index.obj);
      compileExpr(c, target->index.key);
      msChunkEmitOp(c->chunk, OP_SET_ITEM, line);
      break;
    case MS_ND_TUPLE:
      compilerError(c, n->pos, "tuple unpack assignment not yet implemented");
      break;
    default:
      compilerError(c, n->pos, "invalid assignment target");
      break;
  }
  msChunkEmitOp(c->chunk, OP_POP, line);
}

static MsOpCode compoundOpToOpCode(MsTokKind tok) {
  switch (tok) {
    case MS_TOK_PLUS_ASSIGN:
      return OP_ADD;
    case MS_TOK_MINUS_ASSIGN:
      return OP_SUB;
    case MS_TOK_STAR_ASSIGN:
      return OP_MUL;
    case MS_TOK_SLASH_ASSIGN:
      return OP_DIV;
    case MS_TOK_PERCENT_ASSIGN:
      return OP_MOD;
    case MS_TOK_AMP_ASSIGN:
      return OP_BAND;
    case MS_TOK_PIPE_ASSIGN:
      return OP_BOR;
    case MS_TOK_CARET_ASSIGN:
      return OP_BXOR;
    case MS_TOK_SHL_ASSIGN:
      return OP_SHL;
    case MS_TOK_SHR_ASSIGN:
      return OP_SHR;
    default:
      assert(0 && "unreachable: invalid compound op");
      return OP_ADD;
  }
}

static void compileCompoundAssign(MsCompiler* c, MsNode* n) {
  MsNode* target = n->binary.left;
  uint32_t line = n->pos.line;
  if (target->kind != MS_ND_IDENT) {
    compilerError(c, n->pos, "compound assignment only supports identifiers");
    return;
  }
  VarLoc loc = resolveVar(c, target->ident.name, target->ident.len);
  emitGetVar(c, loc, line);
  compileExpr(c, n->binary.right);
  MsOpCode op = compoundOpToOpCode(n->binary.op);
  msChunkEmitOp(c->chunk, op, line);
  emitSetVar(c, loc, line);
  msChunkEmitOp(c->chunk, OP_POP, line);
}

static void compileDel(MsCompiler* c, MsNode* n) {
  MsNode* target = n->singleExpr.expr;
  uint32_t line = n->pos.line;
  switch (target->kind) {
    case MS_ND_IDENT: {
      if (msScopeResolveLocal(c, target->ident.name, target->ident.len) >= 0 ||
          msScopeResolveUpvalue(c, target->ident.name, target->ident.len) >= 0) {
        compilerError(c, n->pos, "cannot del local variable in current version");
        return;
      }
      uint32_t nameIdx = addStringConst(c, target->ident.name, target->ident.len);
      msChunkEmitOpAX(c->chunk, OP_DEL_GLOBAL, nameIdx, line);
      break;
    }
    case MS_ND_ATTR: {
      compileExpr(c, target->attr.obj);
      uint32_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
      msChunkEmitOpAX(c->chunk, OP_DEL_ATTR, nameIdx, line);
      break;
    }
    case MS_ND_INDEX:
      compileExpr(c, target->index.obj);
      compileExpr(c, target->index.key);
      msChunkEmitOp(c->chunk, OP_DEL_ITEM, line);
      break;
    default:
      compilerError(c, n->pos, "invalid del target");
      break;
  }
}

static void compileForIn(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  uint32_t line = n->pos.line;
  msScopeBegin(c);
  compileExpr(c, n->forStmt.forIter);
  msChunkEmitOp(c->chunk, OP_GET_ITER, line);
  // hidden local for iterator
  msScopeDeclareLocal(c, "", 0);
  msScopeMarkInitialized(c);

  loop->loopStart = c->chunk->codeLen;
  uint32_t exitJump = emitJump(c, OP_FOR_ITER, line);

  compileBind(c, n->forStmt.forTarget, line);
  compileBlock(c, n->forStmt.body);

  // continue target: pop iteration var then jump back
  uint32_t continueTarget = c->chunk->codeLen;
  msChunkEmitOp(c->chunk, OP_POP, line);
  c->localCount--;

  for (int i = 0; i < loop->continueCount; i++) {
    patchJumpTo(c, loop->continuePatches[i], continueTarget);
  }

  emitBackJump(c, loop->loopStart, line);
  patchJump(c, exitJump);
  msScopeEnd(c);
}

static void compileWhile(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  uint32_t line = n->pos.line;
  loop->loopStart = c->chunk->codeLen;
  compileExpr(c, n->forStmt.cond);
  uint32_t exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);
  compileBlock(c, n->forStmt.body);

  for (int i = 0; i < loop->continueCount; i++) {
    patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
  }

  emitBackJump(c, loop->loopStart, line);
  patchJump(c, exitJump);
}

static void compileInfiniteLoop(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  uint32_t line = n->pos.line;
  loop->loopStart = c->chunk->codeLen;
  compileBlock(c, n->forStmt.body);

  for (int i = 0; i < loop->continueCount; i++) {
    patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
  }

  emitBackJump(c, loop->loopStart, line);
}

static void compileFor(MsCompiler* c, MsNode* n) {
  MsLoopCtx loop = {.breakCount = 0, .continueCount = 0, .loopStart = 0, .outer = c->currentLoop};
  c->currentLoop = &loop;

  if (n->forStmt.forTarget != NULL) {
    compileForIn(c, n, &loop);
  } else if (n->forStmt.cond != NULL) {
    compileWhile(c, n, &loop);
  } else {
    compileInfiniteLoop(c, n, &loop);
  }

  for (int i = 0; i < loop.breakCount; i++) {
    patchJump(c, loop.breakPatches[i]);
  }
  c->currentLoop = loop.outer;
}

static void compileBreak(MsCompiler* c, MsNode* n) {
  if (!c->currentLoop) {
    compilerError(c, n->pos, "break outside loop");
    return;
  }
  if (c->currentLoop->breakCount >= MS_MAX_LOOP_PATCHES) {
    compilerError(c, n->pos, "too many break statements");
    return;
  }
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  c->currentLoop->breakPatches[c->currentLoop->breakCount++] = patch;
}

static void compileContinue(MsCompiler* c, MsNode* n) {
  if (!c->currentLoop) {
    compilerError(c, n->pos, "continue outside loop");
    return;
  }
  if (c->currentLoop->continueCount >= MS_MAX_LOOP_PATCHES) {
    compilerError(c, n->pos, "too many continue statements");
    return;
  }
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  c->currentLoop->continuePatches[c->currentLoop->continueCount++] = patch;
}

static void compileSwitch(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  bool hasExpr = (n->switchStmt.expr != NULL);
  if (hasExpr) {
    compileExpr(c, n->switchStmt.expr);
  }

  uint32_t endPatches[MS_MAX_LOOP_PATCHES];
  int endCount = 0;
  MsNode* defaultCase = NULL;

  for (MsNodeList* cl = n->switchStmt.cases; cl; cl = cl->next) {
    MsNode* caseNode = cl->node;
    if (caseNode->switchCase.values == NULL) {
      defaultCase = caseNode;
      continue;
    }

    uint32_t matchPatches[MS_MAX_LOOP_PATCHES];
    int matchCount = 0;
    for (MsNodeList* vl = caseNode->switchCase.values; vl; vl = vl->next) {
      if (hasExpr) {
        msChunkEmitOp(c->chunk, OP_DUP, line);
        compileExpr(c, vl->node);
        msChunkEmitOp(c->chunk, OP_EQ, line);
      } else {
        compileExpr(c, vl->node);
      }
      if (matchCount < MS_MAX_LOOP_PATCHES) {
        matchPatches[matchCount++] = emitJump(c, OP_JUMP_IF_TRUE, line);
      }
    }
    uint32_t skipBody = emitJump(c, OP_JUMP, line);

    for (int i = 0; i < matchCount; i++) {
      patchJump(c, matchPatches[i]);
    }

    if (hasExpr) {
      msChunkEmitOp(c->chunk, OP_POP, line);
    }
    compileBlock(c, caseNode->switchCase.body);

    if (endCount < MS_MAX_LOOP_PATCHES) {
      endPatches[endCount++] = emitJump(c, OP_JUMP, line);
    }

    patchJump(c, skipBody);
  }

  if (defaultCase) {
    if (hasExpr) {
      msChunkEmitOp(c->chunk, OP_POP, line);
    }
    compileBlock(c, defaultCase->switchCase.body);
  } else if (hasExpr) {
    msChunkEmitOp(c->chunk, OP_POP, line);
  }

  for (int i = 0; i < endCount; i++) {
    patchJump(c, endPatches[i]);
  }
}

static void compileBind(MsCompiler* c, MsNode* target, uint32_t line) {
  (void) line;
  if (target->kind == MS_ND_IDENT) {
    int slot = msScopeDeclareLocal(c, target->ident.name, target->ident.len);
    if (slot < 0) {
      compilerError(c, target->pos, "too many locals or duplicate in for-in");
      return;
    }
    msScopeMarkInitialized(c);
  } else {
    compilerError(c, target->pos, "only single variable supported in for-in");
  }
}

static void compileBlock(MsCompiler* c, MsNode* n) {
  if (!n) {
    return;
  }
  if (n->kind == MS_ND_BLOCK) {
    msScopeBegin(c);
    for (MsNodeList* s = n->block.stmts; s; s = s->next) {
      compileStmt(c, s->node);
      if (c->result->hadError) {
        return;
      }
    }
    msScopeEnd(c);
  } else {
    compileStmt(c, n);
  }
}

static void compileIf(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->ifStmt.cond);
  uint32_t jumpFalse = emitJump(c, OP_JUMP_IF_FALSE, line);
  compileBlock(c, n->ifStmt.thenBlock);
  if (n->ifStmt.elseBlock) {
    uint32_t jumpEnd = emitJump(c, OP_JUMP, line);
    patchJump(c, jumpFalse);
    compileStmt(c, n->ifStmt.elseBlock);
    patchJump(c, jumpEnd);
  } else {
    patchJump(c, jumpFalse);
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
    case MS_ND_VAR_DECL:
    case MS_ND_SHORT_DECL:
      compileVarDecl(c, node);
      break;
    case MS_ND_ASSIGN:
      compileAssign(c, node);
      break;
    case MS_ND_COMPOUND_ASSIGN:
      compileCompoundAssign(c, node);
      break;
    case MS_ND_DEL:
      compileDel(c, node);
      break;
    case MS_ND_IF:
      compileIf(c, node);
      break;
    case MS_ND_FOR:
      compileFor(c, node);
      break;
    case MS_ND_SWITCH:
      compileSwitch(c, node);
      break;
    case MS_ND_BREAK:
      compileBreak(c, node);
      break;
    case MS_ND_CONTINUE:
      compileContinue(c, node);
      break;
    case MS_ND_BLOCK:
      compileBlock(c, node);
      break;
    case MS_ND_PASS:
      break;
    case MS_ND_FUNC_DECL:
    case MS_ND_ASYNC_FUNC:
      compileFuncDecl(c, node);
      break;
    case MS_ND_CLASS_DECL:
      compileClassDecl(c, node);
      break;
    case MS_ND_RETURN:
      compileReturn(c, node);
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
