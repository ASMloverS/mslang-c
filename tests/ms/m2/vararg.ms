// vararg: ...args collection + call-site ...iterable expansion (T069)
func sum(first, ...nums) {
    total := first
    for n in nums { total += n }
    return total
}
print(sum(1, 2, 3, 4, 5))   // 15

// expand call (call-site spread uses '*', unlike the '...' vararg param)
lst := [10, 20, 30]
print(sum(*lst))            // 60

// mixed: fixed params + vararg param + expand call
func f(a, b, ...rest) { return a + b + sum(*rest) }
print(f(1, 2, 3, 4))         // 10
