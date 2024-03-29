#include <Common.h>

namespace ftp {

    void ConnectionBase::start() {
        _addrLen = sizeof(_remoteAddr);
        //Notify the parent that we are waiting for connection.
        _ring->async_sock_accept(_fd, reinterpret_cast<sockaddr *>(&_remoteAddr), &_addrLen, 0, [this](int res) {
            _fd = res;
            if (_fd < 0) {
                //stop self
                stop();
            } else {
                //After all queue manipulations, we can finally start the protocol payload functioning.
                startActing();
            }
        });
    }

    void ConnectionBase::stop() {
        std::shared_ptr<ConnectionBase> child;
        while (!_childConnections.empty()) {
            {
                auto lk = std::lock_guard(_childConnectionsMutex);
                if (!_childConnections.empty()) {
                    child = _childConnections.back();
                    _childConnections.pop_back();
                }
            }
            child->_parent = nullptr;
            child->stop();
        }
        if (_fd >= 0) {
            close(_fd);
            _fd = -1;
        }
        if (_parent)
            _parent->acceptChildStop(this);
    }

    void ConnectionBase::enqueueConnection(int fd, std::shared_ptr<ConnectionBase> &&connection) {
        socklen_t addrLen = sizeof(sockaddr_in);
        getsockname(fd, reinterpret_cast<sockaddr *>(&(connection->_localAddr)), &addrLen);
        connection->_fd = fd;
        {
            auto lk = std::lock_guard(_childConnectionsMutex);
            _childConnections.push_back(connection);
        }
        connection->start();
    }

    void ConnectionBase::acceptChildStop(ConnectionBase *child) {
        //Adding thread-safety
        auto lk = std::lock_guard(_childConnectionsMutex);
        auto childPtr = *std::find_if(_childConnections.begin(), _childConnections.end(),
                                      [child](const auto &el) { return el.get() == child; });
        childPtr->_parent = nullptr;
        std::erase(_childConnections, childPtr);
    }

}