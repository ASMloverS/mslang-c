// kwargs: keyword arguments + **kwargs collection (T070)
// NOTE: f-string interpolation ($"...{expr}...") is a known, separately
// tracked gap (docs/language/testing-ms.md: Pratt parser reports "expected
// expression" for `{expr}`) -- print()'s space-joined args are used instead.
func configure(host, port=80, ssl=false, timeout=30) {
    print(host, port, ssl, timeout)
}


configure("example.com")
// example.com 80 false 30

configure("example.com", 443, true)
// example.com 443 true 30

configure("example.com", ssl=true, timeout=90)
// example.com 80 true 90


func passthrough(first, **kwargs) {
    return kwargs
}


// map has no tpRepr yet (see tests/ms/m1/map_ops.ms), so verify contents via
// len()/subscript rather than printing the whole map directly.
kw := passthrough(1, a=2, b=3)
print(len(kw))
// 2
print(kw["a"], kw["b"])
// 2 3

// single **expr independent expansion (no literal kwarg mixed in)
opts := {"timeout": 90}
configure("api.example.com", **opts)
// api.example.com 80 false 90

// plain call (no keyword args at all) -- **kwargs must still collect as an
// empty map, not nil
print(len(passthrough(1)))
// 0
