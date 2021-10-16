#ifndef URING_TCP_SERVER_CONTROLCONNECTION_H
#define URING_TCP_SERVER_CONTROLCONNECTION_H

#include <string>
#include <vector>
#include <memory>
#include <arpa/inet.h>
#include <algorithm>
#include <mutex>
#include <Common.h>

namespace ftp {

    using namespace std::string_literals;

    class ControlConnection;

    class ControlConnectionState {

    public:
        explicit ControlConnectionState(ControlConnection* handledConnection): _handledConnection(handledConnection) {}

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

        virtual ~ControlConnectionState() = default;

    protected:
        ControlConnection* _handledConnection;
    };

    enum class DataConnectionMode{
        sender,
        receiver,
        lister
    };

    class DataConnection;

    class ControlConnection: public ConnectionBase {

    public:
        ControlConnection(ConnectionBase* parent,
                      const std::filesystem::path&& root,
                      const std::shared_ptr<FileSystemProxy>&& fileSystem
                      ):
                ConnectionBase(parent),
                _root(root),
                _fileSystem(fileSystem),
                _command(std::make_shared<std::string>()),
                _pasvFD(-1)
        {
            _command->clear();
            _command->reserve(500);
        }

        //Starts an asynchronous FTP ControlConnection
//        void start();

        void processCommand(std::size_t commandLen);

        [[nodiscard]]const std::shared_ptr<AsyncUring>& ring() const { return _ring; }
        const std::filesystem::path& pwd() const noexcept { return _pwd; }
        std::filesystem::path& pwd() noexcept { return _pwd; }
        bool hasChildConnections() noexcept { return !_childConnections.empty(); }
        const std::filesystem::path& root() const noexcept { return _root; }
        void switchState(std::unique_ptr<ControlConnectionState>&& state) noexcept {
            _pwd = "";
            _state = std::move(state);
        }
        std::shared_ptr<FileSystemProxy> fileSystem() { return _fileSystem; }

        void postDataSendTask(std::filesystem::path&& path, DataConnectionMode mode, std::function<void()>&& dataTransferEndCallback);

        void makePasv();

        void setStructure(FileStructure structure) noexcept { _structure = structure; }

        Callback defaultAsyncOpHandler = [this](std::size_t res){
            if(res == -1) stop();
            _ring->async_read_until(
                    _fd,
                    std::move(_command),
                    "\r\n",
                    [this](std::size_t commandLen){ processCommand(commandLen); }
            );
        };

        void stop() override {
            if(_pasvFD >= 0)
                close(_pasvFD);
            ConnectionBase::stop();
        }

    protected:

        void startActing() override;

    private:
        std::unique_ptr<ControlConnectionState> _state;
        std::filesystem::path _pwd;
        std::filesystem::path _root;
        std::shared_ptr<std::string> _command;
        RepresentationType _type;
        FileStructure _structure;
        TransferMode _mode;
        int _pasvFD;
        std::shared_ptr<DataConnection> _currentPasvChild;
        std::shared_ptr<FileSystemProxy> _fileSystem;
    };

    class DataConnection: public ConnectionBase{
    public:

        DataConnection(ConnectionBase* parent,
                       std::shared_ptr<FileSystemProxy>&& fileSystem,
                       std::function<void(void)>&& waitingForConnectionCallback = [](){}
        );

        void command(std::filesystem::path&& pathToFile,
                     DataConnectionMode mode,
                     std::function<void(void)>&& dataTransmissionEndCallback);

    protected:

        void startActing() override {}

        Callback continue_transmission;

    private:
        std::filesystem::path _pathToFile;
        std::shared_ptr<FileSystemProxy> _fileSystem;
        DataConnectionMode _mode;
        std::function<void(void)> _dataTransmissionEndCallback;
        int _fileFd;
        FILE* _fileStruct;
        std::shared_ptr<std::string> _buffer;
        std::uint64_t _bytesRead;
    };

}

#endif //URING_TCP_SERVER_CONTROLCONNECTION_H
