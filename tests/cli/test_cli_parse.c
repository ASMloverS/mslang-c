#include "ms_test.h"
#include "mslang/ms_cli.h"

static void testImplicitRun(void) {
  char* argv[] = {"mslang", "script.ms", NULL};
  struct MsCliCtx ctx;
  int r = cliParse(2, argv, &ctx);
  MS_ASSERT_EQ(r, 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_RUN, "implicit run");
  MS_ASSERT_STR_EQ(ctx.script, "script.ms", "script path");
}

static void testExplicitTokensCmd(void) {
  char* argv[] = {"mslang", "tokens", "foo.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_TOKENS, "tokens cmd");
  MS_ASSERT_STR_EQ(ctx.script, "foo.ms", "script path");
}

static void testFlags(void) {
  char* argv[] = {"mslang", "-B", "--hash-cache", "-v", "run", "a.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(6, argv, &ctx), 0, "parse ok");
  MS_ASSERT_TRUE(ctx.flags.noCache,   "noCache");
  MS_ASSERT_TRUE(ctx.flags.hashCache, "hashCache");
  MS_ASSERT_TRUE(ctx.flags.verbose,   "verbose");
}

static void testScriptArgs(void) {
  char* argv[] = {"mslang", "run", "s.ms", "a", "b", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(5, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.scriptArgc, 2, "scriptArgc");
  MS_ASSERT_STR_EQ(ctx.scriptArgv[0], "a", "argv[0]");
}

static void testNoCache(void) {
  char* argv[] = {"mslang", "--no-cache", "script.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_RUN, "implicit run");
  MS_ASSERT_TRUE(ctx.flags.noCacheRead, "noCacheRead");
}

static void testExplicitRun(void) {
  char* argv[] = {"mslang", "run", "script.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_RUN, "explicit run");
  MS_ASSERT_STR_EQ(ctx.script, "script.ms", "script path");
}

static void testDisasmMs(void) {
  char* argv[] = {"mslang", "disasm", "file.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_DISASM, "disasm cmd");
  MS_ASSERT_STR_EQ(ctx.script, "file.ms", "script path");
}

static void testDisasmMsc(void) {
  char* argv[] = {"mslang", "disasm", "file.msc", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_DISASM, "disasm msc cmd");
  MS_ASSERT_STR_EQ(ctx.script, "file.msc", "script path");
}

static void testParseCmd(void) {
  char* argv[] = {"mslang", "parse", "foo.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(3, argv, &ctx), 0, "parse ok");
  MS_ASSERT_EQ(ctx.cmd, CLI_CMD_PARSE, "parse cmd");
}

static void testFlagsAfterCmd(void) {
  // Flags after sub-command but before script should also be parsed.
  char* argv[] = {"mslang", "run", "-v", "a.ms", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(4, argv, &ctx), 0, "parse ok");
  MS_ASSERT_TRUE(ctx.flags.verbose, "verbose after cmd");
  MS_ASSERT_STR_EQ(ctx.script, "a.ms", "script");
}

static void testNoArgsReturnsError(void) {
  char* argv[] = {"mslang", NULL};
  struct MsCliCtx ctx;
  MS_ASSERT_EQ(cliParse(1, argv, &ctx), -1, "no args => error");
}

int main(void) {
  MS_RUN(testImplicitRun);
  MS_RUN(testExplicitTokensCmd);
  MS_RUN(testFlags);
  MS_RUN(testScriptArgs);
  MS_RUN(testNoCache);
  MS_RUN(testExplicitRun);
  MS_RUN(testDisasmMs);
  MS_RUN(testDisasmMsc);
  MS_RUN(testParseCmd);
  MS_RUN(testFlagsAfterCmd);
  MS_RUN(testNoArgsReturnsError);
  return msTestSummary();
}
