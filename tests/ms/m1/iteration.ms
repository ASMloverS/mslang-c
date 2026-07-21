// for-in over list/tuple/str/map/set/range + break/continue (T065)
for v in [1, 2, 3] {
    print(v)
}

for ch in "abc" {
    print(ch)
}

for k in {"x": 1, "y": 2} {
    print(k)
}

for e in (10, 20, 30) {
    print(e)
}

// break/continue
out := []
for i in range(6) {
    if i == 1 { continue }
    if i == 4 { break }
    out += [i]
}
print(out)

// nested loops
for i in range(2) {
    for j in range(2) {
        print(i, j)
    }
}

// iterate a slice
for v in [1, 2, 3, 4, 5][1:4] {
    print(v)
}
