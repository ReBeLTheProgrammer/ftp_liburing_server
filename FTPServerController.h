#ifndef URING_TCP_SERVER_FTPSERVERCONTROLLER_H
#define URING_TCP_SERVER_FTPSERVERCONTROLLER_H

#include <string>
#include <thread>
#include <memory>
#include "async_uring.h"
#include "FTPConnection.h"
#include "FTPCommon.h"
#include <boost/asio/thread_pool.hpp>
#include <boost/intrusive/set.hpp>

namespace mp {

    class FTPServerController: public FTPConnectionBase {
    public:
        //constructs the server, making it dispatch some path (by default, the current path) with given thread count.
        FTPServerController(int fd, sockaddr_in localAddress, const std::filesystem::path &ftpRootPath = std::filesystem::current_path(), int threadCount = std::thread::hardware_concurrency()):
                FTPConnectionBase(fd,
                                  std::make_shared<FTPConnectionQueueType>(),
                                  std::make_shared<std::map<int, int>>(),
                                  std::make_shared<std::mutex>(),
                                  std::make_shared<std::mutex>(),
                                  localAddress,
                                  std::make_shared<uring_wrapper>(512)
                                  ),
                                  _ftpRoot(ftpRootPath),
                                  _fileSystem(std::make_shared<FTPFileSystem>(ftpRootPath)),
                                  _threadPool(threadCount)
        {
            if(!std::filesystem::exists(ftpRootPath))
                throw std::runtime_error("Specified path does not exist");
        }

        //starts the server in current thread and locking it.
        void start(){
            connectionEstablishedCallback();
            _threadPool.executor().post([this](){
                while(true) {
                    _threadPool.executor().post(_ring->check_act(), std::allocator<void>());
                }
                }, std::allocator<void>());
        }
        //Server can be stopped by either call of the inherited stop() method or destruction.

    private:

        std::function<void(void)> connectionEstablishedCallback = [this](){
            enqueueConnection(_localAddr.sin_port,
                              std::make_shared<FTPConnection>(
                                      std::shared_ptr<FTPConnectionBase>(reinterpret_cast<FTPConnectionBase*>(this)),
                                      std::move(_ftpRoot),
                                      std::move(_fileSystem),
                                      std::move(connectionEstablishedCallback)
                                      )
                                      );
        };

        boost::asio::thread_pool _threadPool;
        std::filesystem::path _ftpRoot;
        std::shared_ptr<mp::FTPFileSystem> _fileSystem;
    };

}

#endif //URING_TCP_SERVER_FTPSERVERCONTROLLER_H
