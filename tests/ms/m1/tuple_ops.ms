// tuple: literal/index/slice/concat/len/in/iteration (T061)
// NOTE: tuple has no tpRepr yet -- print via list(tuple) or by
// indexing/iterating individual elements.
t := (1, 2, 3)
print(t[0])
print(t[-1])
print(len(t))
print(list(t))
print(list(t[1:]))

t2 := (4, 5)
print(list(t + t2))

print(2 in t)
print(9 not in t)

for e in t {
    print(e)
}

nested := ((1, 2), (3, 4))
print(nested[0][1])
