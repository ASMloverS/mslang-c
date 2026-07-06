#pragma once

#include <stdint.h>
#include <stdio.h>

struct MsChunk;

// Disassemble an entire chunk to fp, in the "== <name> ==" block format.
// Does not recurse into nested function protos (func-proto objects don't
// exist yet; see P4-T049).
void msChunkDisasm(const struct MsChunk* chunk, const char* name, FILE* fp);

// Disassemble a single instruction at offset to fp. Returns the number of
// bytes consumed (including the opcode byte).
int msDisasmInstr(const struct MsChunk* chunk, uint32_t offset, FILE* fp);
