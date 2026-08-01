// test_magic_methods.c
// T074: magic-method (dunder) opcode dispatch for arithmetic/comparison/
// subscript/iteration operators on user-defined class instances.
//
// Uses test_method_binding.c's runGlobal() workaround: msCompile()+msVMRun()
// cannot reach a top-level expression's value directly (the compiler always
// appends OP_RETURN_NIL to the top-level chunk, and every expression
// statement is followed by an OP_POP), so bind the value to a global and
// read it back through gVM.mainThread.globals instead.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

static MsValue runGlobal(const char* src, const char* name) {
  msVMInit();
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue runResult = msVMRun(r.chunk);
  MS_ASSERT_TRUE(!MS_IS_ERROR(runResult), "runGlobal: program must run without error");
  MsValue key = msNewStr(name, (uint32_t) strlen(name));
  MsValue v = msMapGet(&gVM, gVM.mainThread.globals, key);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static MsValue run(const char* src) {
  msVMInit();
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testAddDispatch(void) {
  MsValue v = runGlobal(
      "class Vec { func __init__(self, x) { self.x = x } func __add__(self, o) { return self.x + o.x } }\n"
      "r := Vec(1) + Vec(2)",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "Vec(1) + Vec(2) dispatches __add__");
}

static void testReverseAdd(void) {
  MsValue v = runGlobal(
      "class R { func __init__(self, x) { self.x = x } func __radd__(self, o) { return o + self.x } }\n"
      "r := 1 + R(2)",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "1 + R(2) falls back to __radd__");
}

static void testNoDunderIsTypeError(void) {
  MsValue v = run("class Plain {}\nPlain() + Plain()");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "no __add__/__radd__ on either side is a TypeError");
}

static void testEqDispatch(void) {
  MsValue v = runGlobal(
      "class Point { func __init__(self, x, y) { self.x = x self.y = y } "
      "func __eq__(self, o) { return self.x == o.x and self.y == o.y } }\n"
      "r := Point(1, 2) == Point(1, 2)",
      "r");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "Point(1,2) == Point(1,2) dispatches __eq__ -> true");
}

static void testEqMissingFallsBackToIdentity(void) {
  MsValue v = runGlobal(
      "class Plain { func __init__(self, x) { self.x = x } }\n"
      "r := Plain(1) == Plain(1)",
      "r");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "no __eq__: falls back to identity, distinct instances != equal");
}

static void testSubscriptDispatch(void) {
  MsValue v = runGlobal(
      "class Box { func __init__(self) { self.v = 0 } "
      "func __getitem__(self, k) { return self.v + k } "
      "func __setitem__(self, k, val) { self.v = k + val } "
      "func __delitem__(self, k) { self.v = 0 - k } }\n"
      "b := Box()\n"
      "b[10] = 5\n"
      "r := b[1]\n"
      "del b[7]\n"
      "r2 := b.v",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 16, "b[10]=5 sets v=15, then b[1] dispatches __getitem__ -> 16");
}

static void testDelitemDispatch(void) {
  MsValue v = runGlobal(
      "class Box { func __init__(self) { self.v = 0 } "
      "func __delitem__(self, k) { self.v = 0 - k } }\n"
      "b := Box()\n"
      "del b[7]\n"
      "r := b.v",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == -7, "del b[7] dispatches __delitem__, leaving stack balanced");
}

// Regression: wrong-arity __eq__ (arity 1, dispatch calls it with 2 args
// self+other) must fail msClosureCall and unwind the pushed argv before
// OP_EQ's fallback path pushes msValueEqual's result -- otherwise the 2
// leaked argv slots permanently offset t->sp in the live frame, and the
// local declared right after (y) silently reads back a stray stack slot
// instead of its own initializer.
static void testEqArityMismatchDoesNotCorruptLaterLocals(void) {
  MsValue v = runGlobal(
      "class E { func __eq__(self) { return true } }\n"
      "func f() {\n"
      "  x := E() == E()\n"
      "  y := 42\n"
      "  return y\n"
      "}\n"
      "r := f()",
      "r");
  MS_ASSERT_TRUE(
      MS_IS_INT(v) && MS_AS_INT(v) == 42,
      "wrong-arity __eq__ falls back to identity without leaking argv slots onto a later local");
}

static void testIterProtocolDispatch(void) {
  MsValue v = runGlobal(
      "class Countdown { func __init__(self, n) { self.n = n } "
      "func __iter__(self) { return self } "
      "func __next__(self) { if self.n <= 0 { return nil } self.n = self.n - 1 return self.n + 1 } }\n"
      "sum := 0\n"
      "for x in Countdown(3) { sum = sum + x }\n"
      "r := sum",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 6, "for x in Countdown(3) dispatches __iter__/__next__, sums to 6");
}

int main(void) {
  MS_RUN(testAddDispatch);
  MS_RUN(testReverseAdd);
  MS_RUN(testNoDunderIsTypeError);
  MS_RUN(testEqDispatch);
  MS_RUN(testEqMissingFallsBackToIdentity);
  MS_RUN(testEqArityMismatchDoesNotCorruptLaterLocals);
  MS_RUN(testSubscriptDispatch);
  MS_RUN(testDelitemDispatch);
  MS_RUN(testIterProtocolDispatch);
  return msTestSummary();
}
