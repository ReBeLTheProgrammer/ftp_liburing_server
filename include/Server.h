#ifndef URING_TCP_SERVER_SERVER_H
#define URING_TCP_SERVER_SERVER_H

#include <string>
#include <thread>
#include <memory>
#include <ControlConnection.h>
#include <boost/asio/thread_pool.hpp>
#include <boost/intrusive/set.hpp>

namespace ftp {

    class Server: public ConnectionBase {
    public:
        //constructs the server, making it dispatch some path (by default, the current path) with given thread count.
        Server(sockaddr_in localAddress, const std::filesystem::path &ftpRootPath = std::filesystem::current_path(), int threadCount = std::thread::hardware_concurrency()):
                ConnectionBase(0,
                               localAddress,
                               std::make_shared<AsyncUring>(1ULL << 12)
                                  ),
                _ftpRoot(ftpRootPath),
                _fileSystem(std::make_shared<FileSystemProxy>(ftpRootPath)),
                _threadPool(threadCount)
        {
            if(!std::filesystem::exists(ftpRootPath))
                throw std::runtime_error("Specified path does not exist");
            _threadPool.executor().post([this](){
                while(_fd > -1) {
                    _threadPool.executor().post( _ring->check_act(), std::allocator<void>());
                }
            }, std::allocator<void>());
        }

        //starts the server in current thread and locking it.
        void start() override {
            do {
                _fd = socket(AF_INET, SOCK_STREAM, 0);
            } while (_fd == -1);
            if (bind(_fd, reinterpret_cast<sockaddr *>(&_localAddr), sizeof(_localAddr)))
                throw std::system_error(errno, std::system_category());
            listen(_fd, 20);
            enqueueConnection(_fd,
                              std::make_shared<ControlConnection>(
                                      this,
                                      std::move(_ftpRoot),
                                      std::move(_fileSystem)
                              )
            );
        };

        //Server can be stopped by either call of the inherited stop() method or destruction.

        void stop() override {
            ConnectionBase::stop();
            _threadPool.stop();
        }

        void startActing() override {};

    private:

        boost::asio::thread_pool _threadPool;
        std::filesystem::path _ftpRoot;
        std::shared_ptr<FileSystemProxy> _fileSystem;
    };

}

#endif //URING_TCP_SERVER_SERVER_H
