#ifndef URING_TCP_SERVER_COMMON_H
#define URING_TCP_SERVER_COMMON_H

#include <queue>
#include <sstream>
#include <functional>
#include <iostream>
#include <FileSystemProxy.h>
#include <AsyncUring.h>

namespace ftp {

    class ConnectionBase{
    public:

        ConnectionBase(int fd,
                       sockaddr_in localAddr,
                       std::shared_ptr<AsyncUring>&& ring
                          ):
        _fd(fd),
        _childConnectionsMutex(),
        _parent(nullptr),
        _localAddr(localAddr),
        _ring(ring){}

        ConnectionBase(ConnectionBase* parent
                          ):
                _fd(parent->_fd),
                _childConnectionsMutex(),
                _parent(parent),
                _localAddr(parent->_localAddr),
                _ring(parent->_ring) {}

        virtual void start();

        sockaddr_in localAddr() const noexcept {
            return _localAddr;
        }

        int fd() const noexcept{
            return _fd;
        }

        //Kills the connection with all child connections.
        virtual void stop();

        void enqueueConnection(int fd, std::shared_ptr<ConnectionBase>&& connection);

        virtual ~ConnectionBase(){
            ConnectionBase::stop();
        }

    protected:
        int _fd;
        sockaddr_in _localAddr, _remoteAddr;
        socklen_t _addrLen;
        std::vector<std::shared_ptr<ConnectionBase>> _childConnections;
        std::mutex _childConnectionsMutex;
        ConnectionBase* _parent;
        std::shared_ptr<AsyncUring> _ring;

        virtual void startActing() = 0;

        //To keep the children list up to date, we need to find and erase the closed child from the children list
        void acceptChildStop(ConnectionBase* child);

    };

    enum class TransferMode: char{
        Stream = 'S'
    };
    enum class RepresentationType: char{
        ASCII = 'A',
        NonPrint = 'N',
        Image = 'I'
    };
    enum class FileStructure: char{
        File = 'F',
        Record = 'R'
    };

}

#endif //URING_TCP_SERVER_COMMON_H
