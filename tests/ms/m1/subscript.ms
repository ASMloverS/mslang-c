// item get/set/del dispatch via MsType slots: OP_GET/SET_ITEM, OP_DEL_ITEM (T066)
lst := [1, 2, 3]
print(lst[1])
lst[1] = 99
print(lst)

m := {"a": 1}
print(m["a"])
m["b"] = 2
print(m["b"])
del m["a"]
print("a" in m)
print(len(m))

b := b"hi"
print(b[0])
b[0] = 72
print(b[0])

// negative index get/set
lst2 := [1, 2, 3]
print(lst2[-1])
lst2[-1] = 30
print(lst2)
