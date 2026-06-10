// CLI argument parsing and sub-command dispatch.
#include "mslang/ms_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Return 1 if the string ends with the given suffix, 0 otherwise.
static int endsWith(const char* s, const char* suffix) {
  size_t sl = strlen(s);
  size_t xl = strlen(suffix);
  if (xl > sl) {
    return 0;
  }
  return strcmp(s + sl - xl, suffix) == 0;
}

// Return 1 if the string looks like a file path (ends with .ms or .msc).
static int isFilePath(const char* s) {
  return endsWith(s, ".ms") || endsWith(s, ".msc");
}

// ---------------------------------------------------------------------------

void cliUsage(void) {
  fprintf(stderr,
    "Usage: mslang [flags] <command> [args]\n"
    "\n"
    "Commands:\n"
    "  run      <script.ms> [args...]  Run a script\n"
    "  compile  <path> [paths...]      Pre-compile .ms files to .msc\n"
    "  disasm   <file.ms|file.msc>     Disassemble bytecode\n"
    "  tokens   <script.ms>            Dump token stream (debug)\n"
    "  parse    <script.ms>            Dump AST (debug)\n"
    "\n"
    "Flags (may appear before or after command):\n"
    "  -B              Disable writing bytecode cache\n"
    "  --no-cache      Skip all cache reads and writes\n"
    "  --hash-cache    Use content-hash cache invalidation\n"
    "  -v              Verbose output\n"
    "  --              Stop flag parsing\n"
    "\n"
    "Implicit run: mslang script.ms [args...]\n"
  );
}

void cliApplyEnv(struct MsCliFlags* flags) {
  const char* v;

  v = getenv("MSLANG_DONT_WRITE_BYTECODE");
  if (v && strcmp(v, "0") != 0 && strcmp(v, "") != 0) {
    flags->noCache = true;
  }

  v = getenv("MSLANG_HASH_CACHE");
  if (v && strcmp(v, "0") != 0 && strcmp(v, "") != 0) {
    flags->hashCache = true;
  }
}

int cliParse(int argc, char** argv, struct MsCliCtx* ctx) {
  // Zero-initialize.
  ctx->cmd        = CLI_CMD_UNKNOWN;
  ctx->flags      = (struct MsCliFlags){false, false, false, false};
  ctx->script     = NULL;
  ctx->scriptArgc = 0;
  ctx->scriptArgv = NULL;

  // Environment variables provide defaults; command-line overrides them.
  cliApplyEnv(&ctx->flags);

  if (argc < 2) {
    cliUsage();
    return -1;
  }

  int i = 1;
  bool stopFlags = false;

  // Scan tokens left-to-right; stop flag parsing after "--" or after script.
  while (i < argc) {
    const char* arg = argv[i];

    if (!stopFlags && strcmp(arg, "--") == 0) {
      stopFlags = true;
      i++;
      continue;
    }

    if (!stopFlags && arg[0] == '-') {
      // Flag.
      if (strcmp(arg, "-B") == 0) {
        ctx->flags.noCache = true;
      } else if (strcmp(arg, "--no-cache") == 0) {
        ctx->flags.noCacheRead = true;
      } else if (strcmp(arg, "--hash-cache") == 0) {
        ctx->flags.hashCache = true;
      } else if (strcmp(arg, "-v") == 0) {
        ctx->flags.verbose = true;
      } else if (strcmp(arg, "--version") == 0) {
        fprintf(stderr, "mslang 0.1.0\n");
        return -1;
      } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
        cliUsage();
        return -1;
      } else {
        fprintf(stderr, "mslang: unknown flag: %s\n", arg);
        cliUsage();
        return -1;
      }
      i++;
      continue;
    }

    // Non-flag token: either sub-command name or implicit run (file path).
    if (ctx->cmd == CLI_CMD_UNKNOWN && !stopFlags) {
      if (strcmp(arg, "run") == 0) {
        ctx->cmd = CLI_CMD_RUN;
        i++;
        continue;
      }
      if (strcmp(arg, "compile") == 0) {
        ctx->cmd = CLI_CMD_COMPILE;
        i++;
        continue;
      }
      if (strcmp(arg, "disasm") == 0) {
        ctx->cmd = CLI_CMD_DISASM;
        i++;
        continue;
      }
      if (strcmp(arg, "tokens") == 0) {
        ctx->cmd = CLI_CMD_TOKENS;
        i++;
        continue;
      }
      if (strcmp(arg, "parse") == 0) {
        ctx->cmd = CLI_CMD_PARSE;
        i++;
        continue;
      }
      if (isFilePath(arg)) {
        // Implicit run.
        ctx->cmd    = CLI_CMD_RUN;
        ctx->script = arg;
        i++;
        // Everything after script is script args.
        ctx->scriptArgc = argc - i;
        ctx->scriptArgv = (i < argc) ? &argv[i] : NULL;
        return 0;
      }
      fprintf(stderr, "mslang: unknown command: %s\n", arg);
      cliUsage();
      return -1;
    }

    // We have a command; first remaining non-flag is the script path.
    if (ctx->script == NULL) {
      ctx->script = arg;
      i++;
      // Everything after script is script args.
      ctx->scriptArgc = argc - i;
      ctx->scriptArgv = (i < argc) ? &argv[i] : NULL;
      return 0;
    }
  }

  // Reached end of argv.
  if (ctx->cmd == CLI_CMD_UNKNOWN) {
    cliUsage();
    return -1;
  }

  // Command given but no script - valid for compile (may have zero paths).
  // Other commands require a script; return success and let cliRun handle it.
  return 0;
}

// ---------------------------------------------------------------------------
// Sub-command stubs (replaced incrementally by later tasks).
// ---------------------------------------------------------------------------

static int cmdRun(struct MsCliCtx* ctx) {
  (void)ctx;
  fprintf(stderr, "not implemented yet\n");
  return 0;
}

static int cmdCompile(struct MsCliCtx* ctx) {
  (void)ctx;
  fprintf(stderr, "not implemented yet\n");
  return 0;
}

static int cmdDisasm(struct MsCliCtx* ctx) {
  (void)ctx;
  fprintf(stderr, "not implemented yet\n");
  return 0;
}

static int cmdTokens(struct MsCliCtx* ctx) {
  (void)ctx;
  fprintf(stderr, "not implemented yet\n");
  return 0;
}

static int cmdParse(struct MsCliCtx* ctx) {
  (void)ctx;
  fprintf(stderr, "not implemented yet\n");
  return 0;
}

int cliRun(struct MsCliCtx* ctx) {
  switch (ctx->cmd) {
    case CLI_CMD_RUN:     return cmdRun(ctx);
    case CLI_CMD_COMPILE: return cmdCompile(ctx);
    case CLI_CMD_DISASM:  return cmdDisasm(ctx);
    case CLI_CMD_TOKENS:  return cmdTokens(ctx);
    case CLI_CMD_PARSE:   return cmdParse(ctx);
    default:
      cliUsage();
      return 1;
  }
}
