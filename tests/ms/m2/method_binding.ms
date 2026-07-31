// method binding: bound method self injection / MRO inheritance lookup (T073)

class Animal {
    func speak(self) {
        return "..."
    }
}


class Dog extends Animal {
    func speak(self) {
        return "Woof!"
    }
}


class Cat extends Animal {
    func speak(self) {
        return "Meow!"
    }
}


dog := Dog()
cat := Cat()
print(dog.speak())   // Woof!
print(cat.speak())   // Meow!


class A {
    func hello(self) {
        return "from A"
    }
}


class B extends A {}


class C extends B {}


print(C().hello())   // from A，MRO: C -> B -> A
