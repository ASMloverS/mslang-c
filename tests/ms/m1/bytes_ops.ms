// bytes: literal/index/len/eq/lt/concat (T058)
// NOTE: bytes has no tpRepr yet, so bytes values are never printed
// directly (only derived scalars: len, indexing, comparisons).
b := b"hello"
print(len(b))
print(b[0])
print(b[4])

b1 := b"ab"
b2 := b"ab"
print(b1 == b2)
print(b1 < b"ac")
print(len(b1 + b"!"))

b[0] = 72
print(b[0])
