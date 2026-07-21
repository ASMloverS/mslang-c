// range: lazy iterator, len/index/in, for-iteration, step (T064)
print(list(range(5)))
print(list(range(2, 7)))
print(list(range(10, 0, -2)))
print(list(range(0, 10, 3)))

r := range(5)
print(len(r))
print(r[0])
print(r[4])
print(3 in r)
print(9 in r)

sum := 0
for i in range(10) {
    sum += i
}
print(sum)
