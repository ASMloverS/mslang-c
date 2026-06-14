#include "parser/ms_arena.h"
#include "mslang/ms_alloc.h"

#include <string.h>

void msArenaInit(struct MsArena* a) {
    a->head = NULL;
    a->ptr  = NULL;
    a->end  = NULL;
}

void* msArenaAlloc(struct MsArena* a, size_t size, size_t align) {
    // enforce minimum alignment of 8
    if (align < 8) {
        align = 8;
    }

    // align current ptr up
    uintptr_t cur = (uintptr_t)a->ptr;
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);

    // fits in current block?
    if (a->head != NULL && aligned + size <= (uintptr_t)a->end) {
        a->ptr = (uint8_t*)(aligned + size);
        return (void*)aligned;
    }

    // allocate a new block
    size_t blockDataSize;
    if (size > MS_ARENA_BLOCK_SIZE / 2) {
        // exact-fit block for large allocations; +align-1 to absorb alignment padding
        blockDataSize = size + (align - 1);
    } else {
        blockDataSize = MS_ARENA_BLOCK_SIZE;
    }

    size_t blockTotal = sizeof(MsArenaBlock) + blockDataSize;
    MsArenaBlock* blk = (MsArenaBlock*)msAlloc(blockTotal);
    blk->cap  = blockDataSize;
    blk->next = a->head;
    a->head   = blk;

    // re-align within the new block
    uintptr_t dataStart = (uintptr_t)blk->data;
    uintptr_t newAligned = (dataStart + (align - 1)) & ~(uintptr_t)(align - 1);
    a->ptr = (uint8_t*)(newAligned + size);
    a->end = blk->data + blockDataSize;

    return (void*)newAligned;
}

void msArenaFree(struct MsArena* a) {
    MsArenaBlock* blk = a->head;
    while (blk != NULL) {
        MsArenaBlock* next = blk->next;
        msFree(blk);
        blk = next;
    }
    a->head = NULL;
    a->ptr  = NULL;
    a->end  = NULL;
}
