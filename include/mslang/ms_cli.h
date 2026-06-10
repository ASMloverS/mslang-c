#ifndef MS_CLI_H
#define MS_CLI_H

#include <stdbool.h>

// Global CLI options (merged from env vars and command-line flags).
struct MsCliFlags {
  bool noCache;      // -B / MSLANG_DONT_WRITE_BYTECODE=1
  bool noCacheRead;  // --no-cache (skip all cache reads and writes)
  bool hashCache;    // --hash-cache / MSLANG_HASH_CACHE=1
  bool verbose;      // -v
};

// Sub-command enum.
typedef enum MsCliCmd {
  CLI_CMD_RUN,
  CLI_CMD_COMPILE,
  CLI_CMD_DISASM,
  CLI_CMD_TOKENS,   // lexer token dump (filled in P1)
  CLI_CMD_PARSE,    // AST dump (filled in P2)
  CLI_CMD_UNKNOWN,
} MsCliCmd;

struct MsCliCtx {
  MsCliCmd          cmd;
  struct MsCliFlags flags;
  const char*       script;      // positional arg: .ms/.msc path, or NULL
  int               scriptArgc;  // count of args after script (sys.argv)
  char**            scriptArgv;  // pointer into original argv after script
};

// Parse argc/argv into ctx. Returns 0 on success, -1 on error (prints usage).
int  cliParse(int argc, char** argv, struct MsCliCtx* ctx);

// Read environment variables and apply to flags (called by cliParse internally).
void cliApplyEnv(struct MsCliFlags* flags);

// Dispatch to the appropriate sub-command handler.
int  cliRun(struct MsCliCtx* ctx);

// Print usage to stderr.
void cliUsage(void);

#endif // MS_CLI_H
