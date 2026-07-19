// if/else
x := 5
if x > 3 {
    print("big")
} else {
    print("small")
}
// big

// for range
sum := 0
for i in range(10) {
    sum += i
}
print(sum)   // 45

// for in list
for v in [1, 2, 3] {
    print(v)
}
// 1 2 3

// while-style
n := 5
fac := 1
for n > 0 {
    fac *= n
    n -= 1
}
print(fac)   // 120
