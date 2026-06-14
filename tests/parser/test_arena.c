#include "ms_test.h"
#include "parser/ms_arena.h"

static void testArenaBasic(void) {
    struct MsArena a;
    msArenaInit(&a);

    int* p1 = MS_ARENA_NEW(&a, int);
    *p1 = 42;
    int* p2 = MS_ARENA_NEW(&a, int);
    *p2 = 99;

    MS_ASSERT_EQ(*p1, 42, "p1 intact");
    MS_ASSERT_EQ(*p2, 99, "p2 intact");
    MS_ASSERT_EQ(((uintptr_t)p1) % 8, 0, "aligned p1");
    MS_ASSERT_EQ(((uintptr_t)p2) % 8, 0, "aligned p2");

    msArenaFree(&a);
}

static void testArenaLargeAlloc(void) {
    struct MsArena a;
    msArenaInit(&a);
    char* big = MS_ARENA_NEWN(&a, char, 70000);
    MS_ASSERT_TRUE(big != NULL, "large alloc non-null");
    big[69999] = 'X';
    MS_ASSERT_EQ(big[69999], 'X', "large alloc writable");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testArenaBasic);
    MS_RUN(testArenaLargeAlloc);
    return msTestSummary();
}
