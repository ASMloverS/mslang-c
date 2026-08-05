// super(): MRO-based parent-class method dispatch (T075)
// NOTE: f-string interpolation ($"...{expr}...") is a known, separately
// tracked gap (docs/language/testing-ms.md) -- string concatenation is used
// instead.

class A {
    func hello(self) {
        return "A"
    }
}


class B extends A {
    func hello(self) {
        return "B," + super().hello()
    }
}


class C extends B {
    func hello(self) {
        return "C," + super().hello()
    }
}


print(C().hello())   // C,B,A


class Animal {
    func __init__(self, name) {
        self.name = name
    }

    func speak(self) {
        return "..."
    }
}


class Dog extends Animal {
    func __init__(self, name, breed) {
        super().__init__(name)
        self.breed = breed
    }

    func speak(self) {
        return "Woof! I'm " + self.name
    }
}


dog := Dog("Rex", "Labrador")
print(dog.name)     // Rex
print(dog.breed)    // Labrador
print(dog.speak())  // Woof! I'm Rex
