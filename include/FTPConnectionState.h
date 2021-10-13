//
// Created by therbl on 8/8/21.
//

#ifndef URING_TCP_SERVER_FTPCONNECTIONSTATE_H
#define URING_TCP_SERVER_FTPCONNECTIONSTATE_H

#include <FTPConnection.h>
#include <string>

namespace mp {

    using namespace std::string_literals;

    class FTPConnectionStateLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateLoggedIn(FTPConnection* handledConnection): FTPConnectionState(handledConnection) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final;
        void cdup() final;
        void port(const std::string& port) final;
        void pasv() final;
        void type(const std::string& typeCode) final;
        void stru(const std::string& structureCode) final;
        void mode(const std::string& modeCode) final;
        void retr(std::filesystem::path path) final;
        void stor(std::filesystem::path path) final;
        void pwd() const final;
        void list(std::filesystem::path path) const final;
    };

    class FTPConnectionStateNotLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateNotLoggedIn(FTPConnection* handledConnection): FTPConnectionState(handledConnection) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final { defaultBehavior(); }
        void cdup() final { defaultBehavior(); }
        void pasv() final { defaultBehavior(); }
        void port(const std::string& port) final { defaultBehavior(); }
        void type(const std::string& typeCode) final { defaultBehavior(); }
        void stru(const std::string& structureCode) final { defaultBehavior(); }
        void mode(const std::string& modeCode) final { defaultBehavior(); }
        void retr(std::filesystem::path path) final { defaultBehavior(); }
        void stor(std::filesystem::path path) final { defaultBehavior(); }
        void pwd() const final { defaultBehavior(); }
        void list(std::filesystem::path path) const final { defaultBehavior(); }

        void defaultBehavior() const {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("530 Not Logged In\r\n"s),
                    22,
                    _handledConnection->defaultAsyncOpHandler);
        }

    };
}

#endif //URING_TCP_SERVER_FTPCONNECTIONSTATE_H
