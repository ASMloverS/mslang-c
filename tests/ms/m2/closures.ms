// closures: upvalue open/close runtime semantics (T071)
// NOTE: list.append() is not yet wired to `.method()` call syntax (methods
// dicts land in T072/T073, see ms_list.h's msListAppend doc comment), so
// makeAdders() below builds the list via concatenation (adders + [...])
// instead of the more natural adders.append(...).

// 基础闭包：count 在堆上存活，每次调用递增
func makeCounter() {
    count := 0
    return func() {
        count += 1
        return count
    }
}


counter := makeCounter()
print(counter())  // 1
print(counter())  // 2
print(counter())  // 3


// 共享 upvalue：getter/setter 两个闭包捕获同一个 value
func makePair() {
    value := 0
    getter := func() { return value }
    setter := func(newValue) { value = newValue }
    return (getter, setter)
}


pair := makePair()
pair[1](42)
print(pair[0]())   // 42


// 深层嵌套：upvalue of upvalue
func outer() {
    x := 1
    func middle() {
        func inner() {
            return x   // 经由 middle 的 upvalue 数组间接引用 outer 的 x
        }
        return inner
    }
    return middle
}


print(outer()()())   // 1


// 循环体内每次迭代捕获同一名字的局部变量：各次迭代产生独立 upvalue
// （msScopeEnd 在每次迭代的作用域结束处发射 OP_CLOSE_UPVALUE）
func makeAdders() {
    adders := []
    for i in range(3) {
        n := i
        adders = adders + [func() { return n }]
    }
    return adders
}


for adder in makeAdders() {
    print(adder())   // 0, 1, 2（而非全部输出 2）
}
