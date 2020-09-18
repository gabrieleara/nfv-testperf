#ifndef AUTOFACTORY_HPP
#define AUTOFACTORY_HPP

#include <string>
#include <vector>

// NOTICE: using raw pointers for C interoperability, a proper autofactory class
// should use unique or shared pointers

template <class stub, class... c_args> class autofactory {
protected:
    using stubptr = stub *;

public:
    /**
     * Creates a new stub object of the matching type using the supplied
     * parameters.
     * FIXME: there may be problems when used in different context than the one
     * I'm using it, did not test it in other scenaiors.
     * */
    static stubptr _create(const std::string &&type, c_args... args) {
        for (auto p : _protos()) {
            if (p->_match(type)) {
                return p->_factory_create(std::forward<c_args>(args)...);
            }
        }

        return nullptr;
    }

    virtual ~autofactory() = default;

protected:
    /**
     * Returns true of the type string equals the fully qualified name of the
     * given stub class.
     * */
    virtual bool _match(const std::string &type) const = 0;

    /**
     * Creates a new empty (and maybe invalid) stub object of the current type.
     * */
    virtual stubptr _create_empty() const = 0;

    /**
     * Creates a new stub object of the current type.
     * */
    virtual stubptr _factory_create(c_args... args) const = 0;

    /**
     * Returns a reference to the vector of prototypes of all known stub
     * implementations.
     * */
    static std::vector<stubptr> &_protos() {
        static std::vector<stubptr> _prototypes;
        return _prototypes;
    }
};

// Defines a static instance that will act as factory and an initializer object
// that will run some code at loading time, this will push the _factory element
// in the _protos vector
#define AUTOFACTORY_CONCRETE(atype)                                            \
    static atype _factory;                                                     \
    class init_class {                                                         \
    public:                                                                    \
        init_class() { _protos().push_back(&_factory); }                       \
    };                                                                         \
    static init_class _init;

#define AUTOFACTORY_DEFINE(atype)                                              \
    atype atype::_factory{};                                                   \
    atype::init_class atype::_init{};

#endif // AUTOFACTORY_HPP
