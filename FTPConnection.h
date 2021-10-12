#ifndef URING_TCP_SERVER_FTPCONNECTION_H
#define URING_TCP_SERVER_FTPCONNECTION_H

#include <string>
#include <vector>
#include <memory>
#include <arpa/inet.h>
#include <algorithm>
#include <mutex>
#include "FTPCommon.h"
#include "async_uring.h"
namespace mp {

    using namespace std::string_literals;

    class FTPConnection;

    class FTPConnectionState {

    public:
        explicit FTPConnectionState(std::shared_ptr<FTPConnection>&& handledConnection): _handledConnection(handledConnection) {}

        virtual void user(const std::string& username) = 0;
        virtual void cwd(std::string path) = 0;
        virtual void cdup() = 0;
        virtual void quit() = 0;
        virtual void pasv() = 0;
        virtual void port(const std::string& port) = 0;
        virtual void type(const std::string& typeCode) = 0;
        virtual void stru(const std::string& structureCode) = 0;
        virtual void mode(const std::string& modeCode) = 0;
        virtual void retr(std::filesystem::path path) = 0;
        virtual void stor(std::filesystem::path path) = 0;
        virtual void pwd() const = 0;
        virtual void list(std::filesystem::path path) const = 0;
        virtual void noop() const final;

        virtual ~FTPConnectionState() = default;

    protected:
        std::shared_ptr<FTPConnection> _handledConnection;
    };

    enum class ConnectionMode{
        sender,
        receiver,
        lister
    };

    class FTPConnectionDataSender;

    class FTPConnection: public FTPConnectionBase {

    public:
        FTPConnection(std::shared_ptr<FTPConnectionBase>&& parent,
                      const std::filesystem::path&& root,
                      const std::shared_ptr<FTPFileSystem>&& fileSystem,
                      std::function<void(void)>&& waitingForConnectionCallback = [](){}
                      ):
                      FTPConnectionBase(std::move(parent),
                                        std::move(waitingForConnectionCallback)),
                                        _root(root),
                                        _fileSystem(fileSystem),
                                        _command(std::make_shared<std::string>()),
                                        _pasvFD(-1){
            //TODO: Make handling of _pasvFD == -1 when enqueueing a connection
            _command->clear();
            _command->reserve(500);
        }

        //Starts an asynchronous FTP Connection
//        void start();

        void processCommand(std::size_t commandLen);

        [[nodiscard]]const std::shared_ptr<uring_wrapper>& ring() const { return _ring; }
        const std::filesystem::path& pwd() const noexcept { return _pwd; }
        std::filesystem::path& pwd() noexcept { return _pwd; }
        bool hasChildConnections() noexcept { return !_childConnections.empty(); }
        const std::filesystem::path& root() const noexcept { return _root; }
        void switchState(std::unique_ptr<FTPConnectionState>&& state) noexcept {
            _pwd = "";
            _state = std::move(state);
        }
        std::shared_ptr<FTPFileSystem> fileSystem() { return _fileSystem; }

        void postDataSendTask(std::filesystem::path&& path, ConnectionMode mode, std::function<void()>&& dataTransferEndCallback);

        void makePasv();

        int pasvFD() const noexcept{
            return _pasvFD;
        }

        const int port() const noexcept {
            return _localAddr.sin_port;
        }

        void setStructure(FTPFileStructure structure) noexcept { _structure = structure; }

        Callback defaultAsyncOpHandler = [this](std::size_t res){
            if(res == -1) stop();
            _ring->async_read_until(
                    _fd,
                    std::move(_command),
                    "\r\n",
                    [this](std::size_t commandLen){ processCommand(commandLen); }
            );
        };

        ~FTPConnection(){
            close(_fd);
        }

    protected:

        void startActing() override;

    private:
        std::unique_ptr<FTPConnectionState> _state;
        std::filesystem::path _pwd;
        std::filesystem::path _root;
        std::shared_ptr<std::string> _command;
        FTPRepresentationType _type;
        FTPFileStructure _structure;
        FTPTransferMode _mode;
        int _pasvFD;
        sockaddr_in _pasvAddr;
        socklen_t _pasvAddrLen;
        std::shared_ptr<FTPConnectionDataSender> _currentPasvChild;
        std::shared_ptr<mp::FTPFileSystem> _fileSystem;
    };

    class FTPConnectionStateNotLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateNotLoggedIn(std::shared_ptr<FTPConnection>&& handledConnection): FTPConnectionState(std::move(handledConnection)) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final { defaultBehavior(); }
        void cdup() final { defaultBehavior(); }
        void quit() final { defaultBehavior(); }
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

    class FTPConnectionStateLoggedIn: public FTPConnectionState{
    public:
        explicit FTPConnectionStateLoggedIn(std::shared_ptr<FTPConnection>&& handledConnection): FTPConnectionState(std::move(handledConnection)) {}

        void user(const std::string& username) final;
        void cwd(std::string path) final;
        void cdup() final;
        void quit() final;
        void port(const std::string& port) final;
        void pasv() final;
        void type(const std::string& typeCode) final;
        void stru(const std::string& structureCode) final;
        void mode(const std::string& modeCode) final;
        void retr(std::filesystem::path path) final;
        void stor(std::filesystem::path path) final;
        void pwd() const final;
        void list(std::filesystem::path path) const final;

    private:
        std::string _systemCommandResult;
    };

    class FTPConnectionDataSender: public FTPConnectionBase{
    public:

        FTPConnectionDataSender(std::shared_ptr<FTPConnectionBase>&& parent,
                                std::shared_ptr<FTPFileSystem>&& fileSystem,
                                std::function<void(void)>&& waitingForConnectionCallback = [](){}
                                ):
                                FTPConnectionBase(std::move(parent),
                                                  std::move(waitingForConnectionCallback)
                                                  ),
                                                  _fileSystem(fileSystem){};

        void command(std::filesystem::path&& pathToFile,
                     ConnectionMode mode,
                     std::function<void(void)>&& dataTransmissionEndCallback);

    protected:

        void startActing() override {}

        Callback continue_transmission = [this](std::int64_t res){
            _buffer->clear();
            _buffer->resize(500);
            if(res < 0){
                //previous socket/file operation failed - assume it is closed.
                if(_mode != ConnectionMode::lister)
                    _fileSystem->close(_fileFd);
                else
                    pclose(_fileStruct);
                _dataTransmissionEndCallback();
                stop();
            } else {
                if(_mode == ConnectionMode::sender){
                    _ring->async_read_some(_fileFd, std::move(_buffer), [this](int res){
                        if(res > 0){
                            //read from file successful
                            _buffer->resize(_buffer->find_last_not_of(static_cast<char>(0)) + 1);
                            _ring->async_write_some(_fd, std::move(_buffer), continue_transmission);
                        } else {
                            //read from file failed - eof reached
                            _fileSystem->close(_fileFd);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    });
                } else if(_mode == ConnectionMode::receiver) {
                    _ring->async_read_some(_fd, std::move(_buffer), [this](int res){
                        if(res > 0){
                            //read from socket successful
                            _buffer->resize(_buffer->find_last_not_of(static_cast<char>(0)) + 1);
                            _ring->async_write_some(_fileFd, std::move(_buffer), continue_transmission);
                        } else {
                            //read from socket failed - connection closed
                            _fileSystem->close(_fileFd);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    });
                } else{
                    //mode: lister
                    _ring->async_read_some(_fileFd, std::move(_buffer), [this](int res){
                        if(res > 0){
                            //read from file successful
                            _buffer->resize(_buffer->find_last_not_of(static_cast<char>(0)) + 1);
                            _ring->async_write_some(_fd, std::move(_buffer), continue_transmission);
                        } else {
                            //read from file failed - eof reached
                            pclose(_fileStruct);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    });
                }
            }
        };

    private:
        std::filesystem::path _pathToFile;
        std::shared_ptr<FTPFileSystem> _fileSystem;
        ConnectionMode _mode;
        std::function<void(void)> _dataTransmissionEndCallback;
        int _fileFd;
        FILE* _fileStruct;
        std::shared_ptr<std::string> _buffer;
    };

}

#endif //URING_TCP_SERVER_FTPCONNECTION_H
