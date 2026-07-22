// func: basic call, default args, override, recursion (T068 call convention)
func add(a, b) { return a + b }
print(add(3, 4))

func greet(name, prefix="Hello") {
    return prefix + ", " + name + "!"
}
print(greet("world"))
print(greet("you", "Hi"))

func fib(n) {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
print(fib(10))
