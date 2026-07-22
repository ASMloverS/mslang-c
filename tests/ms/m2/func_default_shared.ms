// func: default values are evaluated once at def time and shared across
// calls (Python semantics); list .append() is not available yet (T073), so
// this checks sharing via `is` identity rather than mutation (T068).
sharedDefault := [1, 2]
func f(lst=sharedDefault) { return lst }
a := f()
b := f()
print(a is b)
print(a is sharedDefault)
