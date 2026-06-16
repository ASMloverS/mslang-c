#pragma once

#include <stddef.h>
#include <stdint.h>

#define MS_ARENA_BLOCK_SIZE 65536

typedef struct MsArenaBlock MsArenaBlock;

struct MsArenaBlock {
  MsArenaBlock* next;
  size_t cap;
  uint8_t data[1];  // start of block data (allocation follows header)
};

struct MsArena {
  MsArenaBlock* head;  // current block
  uint8_t* ptr;        // allocation cursor
  uint8_t* end;        // end of current block
};

void msArenaInit(struct MsArena* a);
void* msArenaAlloc(struct MsArena* a, size_t size, size_t align);
void msArenaFree(struct MsArena* a);  // free all blocks

#define MS_ARENA_NEW(arena, T) ((T*) msArenaAlloc((arena), sizeof(T), _Alignof(T)))
#define MS_ARENA_NEWN(arena, T, n) ((T*) msArenaAlloc((arena), sizeof(T) * (n), _Alignof(T)))
