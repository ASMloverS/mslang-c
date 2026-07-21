// map: literal/get/set/del/len/in/iteration (T060)
// NOTE: map has no tpRepr yet, so a whole map value is never printed
// directly -- only via list(map) (keys) or individual item access.
m := {"a": 1, "b": 2, "c": 3}
print(m["a"], m["b"], m["c"])
print(len(m))
print("a" in m)
print("z" in m)

m["d"] = 4
print(len(m))
print(m["d"])

del m["a"]
print(len(m))
print("a" in m)

print(list({"x": 1, "y": 2}))

for k in {"p": 1, "q": 2} {
    print(k)
}
