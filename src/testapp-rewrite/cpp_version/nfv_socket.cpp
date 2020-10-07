#include "nfv_socket.hpp"
#include "config.h"
#include <vector>

#include "nfv_socket.hpp"

std::vector<nfv_socket *> &nfv_socket::_protos() {
    static std::vector<nfv_socket *> _prototypes;
    return _prototypes;
}

nfv_socket *nfv_socket::_create(const std::string &type) {
    for (auto p : nfv_socket::_protos()) {
        if (p->_match(type)) {
            return p->_create_empty();
        }
    }

    return nullptr;
}
