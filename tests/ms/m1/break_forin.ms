// regression: break inside a for-in loop must not leak the hidden
// iterator/loop-var slots onto the stack, or a later for-in loop's
// slot-based local reads get corrupted (impl/P4-T067-vm-e2e-m1.md).
for i in [1, 2, 3] {
    print(i)
    if i == 2 {
        break
    }
}
for j in [10, 20, 30] {
    print(j)
}
