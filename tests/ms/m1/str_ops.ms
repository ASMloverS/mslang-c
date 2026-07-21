// str: concat/repeat/index(byte)/slice/len/in/conversions (T057)
s := "Hello, " + "World!"
print(s)
print(len(s))
print(s[0])
print(s[0:1])
print(s[7:12])
print("ab" * 3)

name := "mslang"
print(name[-1])
print(name[1:4])
print(name[::-1])
print(name[::2])
print("lang" in name)
print("xyz" not in name)

print(int("42"))
print(float("3.14"))
print(str(123))
print(str(3.5))
print(repr("ab"))
print(bool(""))
print(bool("x"))
