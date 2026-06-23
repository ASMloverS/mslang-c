#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_lexer.h"

typedef enum MsNodeKind {
  // Literals
  MS_ND_INT,
  MS_ND_FLOAT,
  MS_ND_STRING,
  MS_ND_BYTES,
  MS_ND_BOOL,
  MS_ND_NIL,
  MS_ND_FSTRING,

  // Identifier
  MS_ND_IDENT,

  // Expressions
  MS_ND_UNARY,
  MS_ND_BINARY,
  MS_ND_IF_EXPR,
  MS_ND_CALL,
  MS_ND_ATTR,
  MS_ND_INDEX,
  MS_ND_SLICE,
  MS_ND_STAR_EXPR,
  MS_ND_DOUBLESTAR_EXPR,

  // Collections
  MS_ND_LIST,
  MS_ND_MAP,
  MS_ND_SET,
  MS_ND_TUPLE,
  MS_ND_LISTCOMP,

  // Channel ops
  MS_ND_MAKE,
  MS_ND_RECV,
  MS_ND_SEND,

  // Assignment / declaration
  MS_ND_VAR_DECL,
  MS_ND_ASSIGN,
  MS_ND_SHORT_DECL,
  MS_ND_COMPOUND_ASSIGN,
  MS_ND_INC_DEC,

  // Statements
  MS_ND_EXPR_STMT,
  MS_ND_BLOCK,
  MS_ND_IF,
  MS_ND_FOR,
  MS_ND_SWITCH,
  MS_ND_RETURN,
  MS_ND_BREAK,
  MS_ND_CONTINUE,
  MS_ND_PASS,
  MS_ND_DEL,
  MS_ND_ASSERT,
  MS_ND_RAISE,
  MS_ND_TRY,
  MS_ND_GO,
  MS_ND_SELECT,
  MS_ND_WITH,
  MS_ND_FALLTHROUGH,

  // Top-level declarations
  MS_ND_FUNC_DECL,
  MS_ND_CLASS_DECL,
  MS_ND_IMPORT,
  MS_ND_ASYNC_FUNC,
  MS_ND_AWAIT,

  // Program root
  MS_ND_PROGRAM,

  MS_ND_COUNT,  // sentinel

  // Auxiliary node kinds (used internally, not in main statement/expression list)
  MS_ND_PARAM = MS_ND_COUNT,
  MS_ND_CATCH_CLAUSE,
  MS_ND_SWITCH_CASE,
  MS_ND_KWARG_PAIR,
} MsNodeKind;

// Forward declaration
typedef struct MsNode MsNode;

typedef struct MsNodeList {
  MsNode* node;
  struct MsNodeList* next;
} MsNodeList;

struct MsNode {
  MsNodeKind kind;
  struct MsSrcPos pos;

  union {
    // MS_ND_INT
    struct {
      int64_t ival;
    } litInt;

    // MS_ND_FLOAT
    struct {
      double fval;
    } litFloat;

    // MS_ND_STRING / MS_ND_BYTES
    struct {
      const char* data;
      uint32_t len;
    } litStr;

    // MS_ND_BOOL
    struct {
      bool bval;
    } litBool;

    // MS_ND_FSTRING
    struct {
      MsNodeList* parts;
    } fstring;

    // MS_ND_IDENT
    struct {
      const char* name;
      uint32_t len;
    } ident;

    // MS_ND_UNARY
    struct {
      MsTokKind op;
      MsNode* operand;
    } unary;

    // MS_ND_BINARY / MS_ND_COMPOUND_ASSIGN
    struct {
      MsTokKind op;
      MsNode* left;
      MsNode* right;
    } binary;

    // MS_ND_IF_EXPR
    struct {
      MsNode* cond;
      MsNode* thenExpr;
      MsNode* elseExpr;
    } ifExpr;

    // MS_ND_CALL
    struct {
      MsNode* callee;
      MsNodeList* args;
      MsNodeList* kwargs;
    } call;

    // MS_ND_ATTR
    struct {
      MsNode* obj;
      const char* name;
      uint32_t nameLen;
    } attr;

    // MS_ND_INDEX
    struct {
      MsNode* obj;
      MsNode* key;
    } index;

    // MS_ND_SLICE
    struct {
      MsNode* obj;
      MsNode* lo;
      MsNode* hi;
      MsNode* step;
    } slice;

    // MS_ND_LIST / MS_ND_SET / MS_ND_TUPLE
    struct {
      MsNodeList* elems;
    } container;

    // MS_ND_STAR_EXPR / MS_ND_DOUBLESTAR_EXPR
    struct {
      MsNode* expr;
    } starExpr;

    // MS_ND_LISTCOMP
    struct {
      MsNode* expr;
      MsNodeList* comprehensions;
    } listComp;

    // MS_ND_MAP
    struct {
      MsNodeList* pairs;
    } map;

    // MS_ND_MAKE
    struct {
      MsNode* capExpr;
    } makeExpr;

    // MS_ND_RECV
    struct {
      MsNode* chanExpr;
    } recv;

    // MS_ND_SEND
    struct {
      MsNode* chanExpr;
      MsNode* val;
    } send;

    // MS_ND_VAR_DECL / MS_ND_SHORT_DECL
    struct {
      const char* name;
      uint32_t nameLen;
      MsNode* init;
    } varDecl;

    // MS_ND_ASSIGN
    struct {
      MsNode* target;
      MsNode* value;
    } assign;

    // MS_ND_INC_DEC
    struct {
      MsNode* target;
      bool isInc;
    } incDec;

    // MS_ND_EXPR_STMT
    struct {
      MsNode* expr;
    } exprStmt;

    // MS_ND_BLOCK
    struct {
      MsNodeList* stmts;
    } block;

    // MS_ND_IF
    struct {
      MsNode* cond;
      MsNode* thenBlock;
      MsNode* elseBlock;
    } ifStmt;

    // MS_ND_FOR
    struct {
      MsNode* init;
      MsNode* cond;
      MsNode* post;
      MsNode* body;
      MsNode* forTarget;
      MsNode* forIter;
    } forStmt;

    // MS_ND_SWITCH
    struct {
      MsNode* expr;
      MsNodeList* cases;
    } switchStmt;

    // MS_ND_RETURN / MS_ND_RAISE / MS_ND_ASSERT / MS_ND_DEL
    struct {
      MsNode* expr;
      MsNode* expr2;
    } singleExpr;

    // MS_ND_BREAK / MS_ND_CONTINUE
    struct {
      const char* label;
    } jump;

    // MS_ND_TRY
    struct {
      MsNode* body;
      MsNodeList* handlers;
      MsNode* finallyBlock;
    } tryStmt;

    // MS_ND_GO
    struct {
      MsNode* call;
    } goStmt;

    // MS_ND_SELECT
    struct {
      MsNodeList* cases;
    } selectStmt;

    // MS_ND_WITH
    struct {
      MsNode* expr;
      const char* asName;
      MsNode* body;
    } withStmt;

    // MS_ND_FUNC_DECL / MS_ND_ASYNC_FUNC
    struct {
      const char* name;
      MsNodeList* params;
      MsNode* body;
      bool isAsync;
    } funcDecl;

    // MS_ND_CLASS_DECL
    struct {
      const char* name;
      MsNode* base;
      MsNodeList* body;
    } classDecl;

    // MS_ND_IMPORT
    struct {
      MsNodeList* path;
      const char* asName;
      MsNodeList* fromNames;
      bool fromImport;
    } importStmt;

    // MS_ND_AWAIT
    struct {
      MsNode* expr;
    } awaitExpr;

    // MS_ND_PROGRAM
    struct {
      MsNodeList* stmts;
      const char* filename;
    } program;

    // MS_ND_PARAM
    struct {
      const char* name;
      uint32_t nameLen;
      MsNode* defaultVal;
      bool isVararg;
      bool isKwarg;
    } param;

    // MS_ND_CATCH_CLAUSE
    struct {
      MsNodeList* typeFilter;
      const char* asName;
      MsNode* body;
    } catchClause;

    // MS_ND_SWITCH_CASE
    struct {
      MsNodeList* values;
      MsNode* body;
      bool isDefault;
    } switchCase;

    // MS_ND_KWARG_PAIR
    struct {
      const char* name;
      uint32_t nameLen;
      MsNode* value;
    } kwargPair;
  };
};

static_assert(sizeof(MsNode) <= 128, "MsNode too large");
