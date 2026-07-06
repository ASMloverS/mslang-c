func outer() {
    var x = 1
    func inner() {
        return x
    }
    return inner
}
