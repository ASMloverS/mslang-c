// test_method_binding.c
// T073: bound method (MsBoundMethodObj) self-injection + MRO method lookup tests.
//
// Same msVMInit()-before-msCompile() ordering as test_class.c: constant-pool
// allocations during compile go through msGCAlloc, which needs gGC.nextGC
// already set to a real threshold.
//
// Uses test_class.c's runGlobal() workaround: msCompile()+msVMRun() cannot
// reach a top-level expression's value directly (the compiler always appends
// OP_RETURN_NIL to the top-level chunk, so a bare `A().f()` statement's
// result would be popped and lost) -- bind the call result to a global and
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

static void testBoundMethodCall(void) {
  MsValue v = runGlobal("class A { func f(self) { return 42 } }\nr := A().f()", "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "A().f() == 42");
}

static void testInheritedMethod(void) {
  MsValue v = runGlobal("class A { func f(self) { return 1 } }\nclass B extends A {}\nr := B().f()", "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 1, "B().f() inherits A.f");
}

static void testMroOverride(void) {
  MsValue v = runGlobal(
      "class A { func f(self) { return 1 } }\n"
      "class B extends A { func f(self) { return 2 } }\n"
      "class C extends B {}\n"
      "r := C().f()",
      "r");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 2, "C().f() resolves to B.f via MRO");
}

int main(void) {
  MS_RUN(testBoundMethodCall);
  MS_RUN(testInheritedMethod);
  MS_RUN(testMroOverride);
  return msTestSummary();
}
