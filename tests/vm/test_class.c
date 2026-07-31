// test_class.c
// T072: class instantiation / __init__ / instance attribute tests.
//
// msVMInit() runs before msCompile() (unlike test_class_compile.c's
// compiler-only helper): msCompile()'s constant-pool allocations go through
// msGCAlloc, and until msVMInit()'s msGCInit() call sets gGC.nextGC to a real
// threshold, gGC.nextGC stays at its zero-initialized value, so every
// allocation trips a collection that sweeps away the not-yet-rooted constant
// just created. Compiling first only "works" for trivial single-constant
// programs; a class with a method needs several live constants
// (name/method-name/proto), so it needs the real threshold in place first.
//
// testInstantiateAndInit/testNilAttrVsMissingAttr compile+run through a bound
// global rather than a top-level expression value, same workaround as
// test_call.c/test_kwargs.c/test_closures.c: msCompile()+msVMRun() cannot
// reach a top-level expression's value in a C unit test (the compiler always
// appends OP_RETURN_NIL to the top-level chunk). testNoInitTakesNoArgs needs
// no such workaround: OP_CALL's arity-mismatch path returns MS_ERROR_VALUE
// directly from eval(), bypassing that trailing OP_RETURN_NIL.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

static MsValue run(const char* src) {
  msVMInit();
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

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

static void testInstantiateAndInit(void) {
  MsValue v = runGlobal(
      "class Foo { func __init__(self, x) { self.x = x } }\n"
      "f := Foo(42)\n"
      "r := f.x",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "Foo(42).x == 42");
}

static void testNilAttrVsMissingAttr(void) {
  MsValue v = runGlobal(
      "class Foo {}\n"
      "f := Foo()\n"
      "f.x = nil\n"
      "r := f.x",
      "r");
  MS_ASSERT_TRUE(MS_IS_NIL(v), "f.x explicitly nil, not AttributeError");
}

static void testNoInitTakesNoArgs(void) {
  MsValue v = run("class Foo {}\nFoo(1)");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "Foo(1) with no __init__ is a TypeError");
}

// Regression for the ctor-frame stack rewind bug: a ctor call's frame->slots
// IS its call base (dispatchClassCall overwrote the callee slot with `inst`
// in place, unlike an ordinary closure frame where slots is one past the
// callee). OP_RETURN/OP_RETURN_NIL used to always rewind to `frame->slots -
// 1`, which for ctor frames rewound one slot too far into the caller's
// still-live stack -- here, the `1` pushed for the list literal before
// Foo(2).x runs. Reads the list elements back out through plain-int globals
// (a/b/c) rather than the list object itself, since runGlobal's msVMShutdown
// frees the GC heap before returning -- a heap object handle would dangle.
static void testCtorInsideListLiteralDoesNotCorruptStack(void) {
  msVMInit();
  const char* src =
      "class Foo { func __init__(self, x) { self.x = x } }\n"
      "lst := [1, Foo(2).x, 3]\n"
      "a := lst[0]\n"
      "b := lst[1]\n"
      "c := lst[2]\n";
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue runResult = msVMRun(r.chunk);
  MS_ASSERT_TRUE(!MS_IS_ERROR(runResult), "program runs without error");
  MsValue a = msMapGet(&gVM, gVM.mainThread.globals, msNewStr("a", 1));
  MsValue b = msMapGet(&gVM, gVM.mainThread.globals, msNewStr("b", 1));
  MsValue c = msMapGet(&gVM, gVM.mainThread.globals, msNewStr("c", 1));
  MS_ASSERT_TRUE(MS_IS_INT(a) && MS_AS_INT(a) == 1, "lst[0] == 1");
  MS_ASSERT_TRUE(MS_IS_INT(b) && MS_AS_INT(b) == 2, "lst[1] == 2");
  MS_ASSERT_TRUE(MS_IS_INT(c) && MS_AS_INT(c) == 3, "lst[2] == 3");
  msVMShutdown();
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testInstantiateAndInit);
  MS_RUN(testNilAttrVsMissingAttr);
  MS_RUN(testNoInitTakesNoArgs);
  MS_RUN(testCtorInsideListLiteralDoesNotCorruptStack);
  return msTestSummary();
}
