// magic methods: operator/subscript/iteration dispatch via dunder lookup (T074)

class Vector {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }

    func __add__(self, other) {
        return Vector(self.x + other.x, self.y + other.y)
    }

    func __mul__(self, n) {
        return Vector(self.x * n, self.y * n)
    }

    func __eq__(self, other) {
        return self.x == other.x and self.y == other.y
    }
}


v1 := Vector(1, 2)
v2 := Vector(3, 4)
v3 := v1 + v2
print(v3.x, v3.y)          // 4 6
v4 := v1 * 3
print(v4.x, v4.y)          // 3 6
print(v1 == Vector(1, 2))  // true


class Countdown {
    func __init__(self, n) {
        self.n = n
    }

    func __iter__(self) {
        return self
    }

    func __next__(self) {
        if self.n <= 0 {
            return nil
        }
        self.n = self.n - 1
        return self.n + 1
    }
}


for x in Countdown(3) {
    print(x)                // 3, 2, 1（各占一行）
}


class Box {
    func __init__(self) {
        self.data = 0
    }

    func __getitem__(self, k) {
        return self.data + k
    }

    func __setitem__(self, k, v) {
        self.data = k + v
    }

    func __delitem__(self, k) {
        self.data = 0 - k
    }
}


box := Box()
box[10] = 5
print(box[1])   // 16 (data=15, +1)
del box[7]
print(box.data) // -7
