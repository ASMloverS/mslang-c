#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_disasm.h"

static void testDisasmNocrash(void) {
  const char* src = "x := 1 + 2\nprint(x)";
  MsCompileResult result = msCompile(src, (uint32_t) strlen(src), "<t>");
  MS_ASSERT_TRUE(!result.hadError, "no error");

  // fmemopen is unavailable on Windows MSVC; capture via tmpfile() instead.
  FILE* out = tmpfile();
  msChunkDisasm(result.chunk, "<t>", out);
  long size = ftell(out);
  rewind(out);
  char buf[4096] = {0};
  fread(buf, 1, (size < 4095 ? (size_t) size : 4095), out);
  fclose(out);

  MS_ASSERT_TRUE(strstr(buf, "OP_ADD") != NULL, "OP_ADD in output");
  MS_ASSERT_TRUE(strstr(buf, "OP_CALL") != NULL, "OP_CALL in output");
  msCompileResultFree(&result);
}

int main(void) {
  MS_RUN(testDisasmNocrash);
  return msTestSummary();
}
