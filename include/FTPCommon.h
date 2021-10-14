#ifndef URING_TCP_SERVER_FTPCOMMON_H
#define URING_TCP_SERVER_FTPCOMMON_H

#include <queue>
#include <sstream>
#include <functional>
#include <iostream>
#include <FTPFileSystem.h>
#include <async_uring.h>

extern std::mutex m;

namespace mp {

    class FTPConnectionBase{
    public:

        FTPConnectionBase(int fd,
                          sockaddr_in localAddr,
                          std::shared_ptr<uring_wrapper>&& ring,
                          std::function<void(void)>&& waitingForConnectionCallback = [](){}
                          ):
        _fd(fd),
        _childConnectionsMutex(),
        _parent(nullptr),
        _localAddr(localAddr),
        _waitingForConnectionCallback(waitingForConnectionCallback),
        _ring(ring){}

        FTPConnectionBase(FTPConnectionBase* parent,
                          std::function<void(void)>&& waitingForConnectionCallback = [](){}
                          ):
                _fd(parent->_fd),
                _childConnectionsMutex(),
                _parent(parent),
                _localAddr(parent->_localAddr),
                _waitingForConnectionCallback(waitingForConnectionCallback),
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

        void enqueueConnection(int fd, std::shared_ptr<FTPConnectionBase>&& connection);

        virtual ~FTPConnectionBase(){
            FTPConnectionBase::stop();
        }

    protected:
        int _fd;
        sockaddr_in _localAddr, _remoteAddr;
        socklen_t _addrLen;
        std::vector<std::shared_ptr<FTPConnectionBase>> _childConnections;
        std::mutex _childConnectionsMutex;
        FTPConnectionBase* _parent;
        std::function<void(void)> _waitingForConnectionCallback;
        std::shared_ptr<uring_wrapper> _ring;

        virtual void startActing() = 0;

        //To keep the children list up to date, we need to find and erase the closed child from the children list
        void acceptChildStop(FTPConnectionBase* child);

    };

    enum class FTPTransferMode: char{
        Stream = 'S'
    };
    enum class FTPRepresentationType: char{
        ASCII = 'A',
        NonPrint = 'N'
    };
    enum class FTPFileStructure: char{
        File = 'F',
        Record = 'R'
    };

}

#endif //URING_TCP_SERVER_FTPCOMMON_H
