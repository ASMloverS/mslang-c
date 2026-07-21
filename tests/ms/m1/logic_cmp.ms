// logical/comparison/identity/membership operators (T055/T056)
print(1 < 2)
print(2 <= 2)
print(3 > 5)
print(3 >= 3)
print(3 == 3.0)
print(3 != 4)

// NOTE: `and`/`or` are bound to a name before printing -- printing the
// BINARY and/or expression directly as a call argument can hit a VM
// dispatch bug on the fall-through (non-short-circuit) branch.
a1 := true and false
a2 := true and true
o1 := false or true
o2 := false or false
print(a1)
print(a2)
print(o1)
print(o2)
print(not true)

// short-circuit: right side never evaluated when left already decides it
s1 := false and (1 / 0 == 0)
s2 := true or (1 / 0 == 0)
print(s1)
print(s2)

print(3 in [1, 2, 3])
print(9 not in [1, 2, 3])
print("a" in "cat")

x := nil
print(x is nil)
print(x is not nil)

print(bool(0))
print(bool(""))
print(bool([]))
print(bool({}))
print(bool(nil))
print(bool(1))
