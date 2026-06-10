// mslang entry point (T004).
#include "mslang/ms_cli.h"

int main(int argc, char** argv) {
  struct MsCliCtx ctx;
  if (cliParse(argc, argv, &ctx) < 0) {
    return 1;
  }
  return cliRun(&ctx);
}
