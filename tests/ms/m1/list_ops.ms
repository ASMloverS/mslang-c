// list: literal/index/slice/set/concat/repeat/len/in/nested (T059)
lst := [3, 1, 4, 1, 5, 9, 2, 6]
print(lst)
print(len(lst))
print(lst[0])
print(lst[-1])
print(lst[1:4])
print(lst[::-1])

lst[0] = 99
print(lst)

print(lst + [7, 8])
print([1, 2] * 3)
print(4 in lst)
print(100 not in lst)

nested := [[1, 2], [3, 4]]
print(nested[0])
print(nested[1][0])
print(len(nested))
