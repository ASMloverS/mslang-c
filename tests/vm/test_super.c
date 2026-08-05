// test_super.c
// T075: super() proxy -- MRO-based parent-class method dispatch,
// compile-time "outside of method" rejection, and AttributeError at the
// top of the MRO chain.
//
// Happy-path tests bind the result to a global (a bare top-level
// expression's value is unreachable directly -- the compiler appends
// OP_RETURN_NIL + OP_POP per statement) and inline the VM lifecycle so the
// string assertion runs before msVMShutdown() frees the GC heap: a str
// value handed back through a runGlobal()-style helper that shuts down
// before returning would dangle (see test_class.c's
// testCtorInsideListLiteralDoesNotCorruptStack). Error-path assertions rely
// on MS_ERROR_VALUE propagating straight out of eval() (no OP_POP reached,
// no heap handle involved), so the simpler run() helper suffices there.
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

// Inlines the msVMInit/msCompile/msVMRun/msMapGet/assert sequence (rather
// than routing through runGlobal()) so the string assertion runs before
// msVMShutdown() frees the GC heap -- runGlobal() shuts down before
// returning, so a returned heap object handle (like a str) would dangle
// (see test_class.c's testCtorInsideListLiteralDoesNotCorruptStack).
static void testSuperMroChain(void) {
  msVMInit();
  const char* src =
      "class A { func hello(self) { return \"A\" } }\n"
      "class B extends A { func hello(self) { return \"B,\" + super().hello() } }\n"
      "class C extends B { func hello(self) { return \"C,\" + super().hello() } }\n"
      "r := C().hello()";
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue runResult = msVMRun(r.chunk);
  MS_ASSERT_TRUE(!MS_IS_ERROR(runResult), "runGlobal: program must run without error");
  MsValue v = msMapGet(&gVM, gVM.mainThread.globals, msNewStr("r", 1));
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "C().hello() returns a str");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 5 && memcmp(s->data, "C,B,A", 5) == 0, "C().hello() == \"C,B,A\" via 3-level super chain");
  msVMShutdown();
  msCompileResultFree(&r);
}

static void testSuperInitCallsParent(void) {
  msVMInit();
  const char* src =
      "class Animal { func __init__(self, name) { self.name = name } }\n"
      "class Dog extends Animal { func __init__(self, name) { super().__init__(name) } }\n"
      "r := Dog(\"Rex\").name";
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MsValue runResult = msVMRun(r.chunk);
  MS_ASSERT_TRUE(!MS_IS_ERROR(runResult), "runGlobal: program must run without error");
  MsValue v = msMapGet(&gVM, gVM.mainThread.globals, msNewStr("r", 1));
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(
      s->len == 3 && memcmp(s->data, "Rex", 3) == 0, "super().__init__() sets self.name via Animal.__init__");
  msVMShutdown();
  msCompileResultFree(&r);
}

static void testSuperOutsideMethodIsCompileError(void) {
  MsCompileResult r = msCompile("super()", 7, "<t>");
  MS_ASSERT_TRUE(r.hadError, "super() outside of any method is a compile error");
  msCompileResultFree(&r);
}

static void testSuperAtMroTopIsAttributeError(void) {
  MsValue v =
      run("class A { func hello(self) { return super().hello() } }\n"
          "A().hello()");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "super().hello() at the top of the MRO (no further parent) is an AttributeError");
}

static void testSuperWithNonObjectSelfIsErrorNotCrash(void) {
  MsValue v =
      run("class A { func hello(self) { return \"A\" } }\n"
          "class B extends A { func hello(self) { self = 5\nreturn super().hello() } }\n"
          "B().hello()");
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "super() with self rebound to a non-object value errors instead of crashing");
}

int main(void) {
  MS_RUN(testSuperMroChain);
  MS_RUN(testSuperInitCallsParent);
  MS_RUN(testSuperOutsideMethodIsCompileError);
  MS_RUN(testSuperAtMroTopIsAttributeError);
  MS_RUN(testSuperWithNonObjectSelfIsErrorNotCrash);
  return msTestSummary();
}
