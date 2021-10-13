#include <FTPCommon.h>

void mp::FTPConnectionBase::start() {
    _addrLen = sizeof(_remoteAddr);
    //Notify the parent that we are waiting for connection.
    _waitingForConnectionCallback();
    _ring->async_sock_accept(_fd, reinterpret_cast<sockaddr*>(&_remoteAddr), &_addrLen, 0, [this](int res){
        _fd = res;
        {
            auto lk = std::lock_guard(m);
            std::cout << "async_sock_accept() returned " << res << '\n';
        }
        if(_fd < 0){
            //stop self
            stop();
        } else{
            //After all queue manipulations, we can finally start the protocol payload functioning.
            startActing();
        }
    });
}

void mp::FTPConnectionBase::stop() {
    {
        auto lk = std::lock_guard(_childConnectionsMutex);
        for (auto &child: _childConnections)
            child->stop();
        close(_fd);
        _fd = -1;
    }
    if(_parent)
        _parent->acceptChildStop(this);
}

void mp::FTPConnectionBase::enqueueConnection(int fd, std::shared_ptr<FTPConnectionBase> &&connection) {
    socklen_t addrLen = sizeof(sockaddr_in);
    getsockname(fd, reinterpret_cast<sockaddr*>(&_localAddr), &addrLen);
    connection->_fd = fd;
    {
        auto lk = std::lock_guard(_childConnectionsMutex);
        _childConnections.push_back(connection);
    }
    connection->start();
}

void mp::FTPConnectionBase::acceptChildStop(mp::FTPConnectionBase *child) {
    //Adding thread-safety
    auto lk = std::lock_guard(_childConnectionsMutex);
    _childConnections.erase(
            std::find_if(
                    _childConnections.begin(),
                    _childConnections.end(),
                    [child](const auto& el){return el.get() == child;})
                    );
}
