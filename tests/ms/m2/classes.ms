// class: instantiation/__init__/instance attrs/type() (T072)

class Animal {
    func __init__(self, name) {
        self.name = name
    }
}


class Dog extends Animal {}


animal := Animal("Rex")
print(animal.name)      // Rex
print(type(animal))     // Animal

dog := Dog("Fido")
print(dog.name)          // Fido，__init__ 沿 baseClass 继承自 Animal
print(type(dog))         // Dog

dog.age = 3
print(dog.age)           // 3
