// set: literal(dedup)/len/in/union/intersect/diff/symdiff/subset (T062)
// NOTE: set has no tpRepr yet -- print via list(set) or len()/in.
s := {1, 2, 2, 3, 3}
print(len(s))
print(1 in s)
print(9 in s)

s1 := {1, 2, 3}
s2 := {2, 3, 4}
print(list(s1 | s2))
print(list(s1 & s2))
print(list(s1 - s2))
print(list(s1 ^ s2))

print({1, 2} <= {1, 2, 3})
print({1, 2, 3} <= {1, 2, 3})
print({1, 2} < {1, 2, 3})
print({1, 2, 3} >= {1, 2})
