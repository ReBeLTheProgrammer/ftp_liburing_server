#ifndef URING_TCP_SERVER_FTPCONNECTION_H
#define URING_TCP_SERVER_FTPCONNECTION_H

#include <string>
#include <vector>
#include <memory>
#include <arpa/inet.h>
#include <algorithm>
#include <mutex>
#include <FTPCommon.h>

namespace mp {

    using namespace std::string_literals;

    class FTPConnection;

    class FTPConnectionState {

    public:
        explicit FTPConnectionState(FTPConnection* handledConnection): _handledConnection(handledConnection) {}

        virtual void user(const std::string& username) = 0;
        virtual void cwd(std::string path) = 0;
        virtual void cdup() = 0;
        virtual void quit() final;
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
        FTPConnection* _handledConnection;
    };

    enum class ConnectionMode{
        sender,
        receiver,
        lister
    };

    class FTPConnectionDataSender;

    class FTPConnection: public FTPConnectionBase {

    public:
        FTPConnection(FTPConnectionBase* parent,
                      const std::filesystem::path&& root,
                      const std::shared_ptr<FTPFileSystem>&& fileSystem,
                      std::function<void(void)>&& waitingForConnectionCallback = [](){}
                      ):
                      FTPConnectionBase(parent,
                                        std::move(waitingForConnectionCallback)),
                                        _root(root),
                                        _fileSystem(fileSystem),
                                        _command(std::make_shared<std::string>())
        {
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
        std::shared_ptr<FTPConnectionDataSender> _currentPasvChild;
        std::shared_ptr<mp::FTPFileSystem> _fileSystem;
    };

    class FTPConnectionDataSender: public FTPConnectionBase{
    public:

        FTPConnectionDataSender(FTPConnectionBase* parent,
                                std::shared_ptr<FTPFileSystem>&& fileSystem,
                                std::function<void(void)>&& waitingForConnectionCallback = [](){}
        );

        void command(std::filesystem::path&& pathToFile,
                     ConnectionMode mode,
                     std::function<void(void)>&& dataTransmissionEndCallback);

    protected:

        void startActing() override {}

        Callback continue_transmission;

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
