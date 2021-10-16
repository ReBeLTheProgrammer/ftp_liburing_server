#include <FTPConnection.h>
#include <FTPConnectionState.h>

#include <memory>
#include <cstdio>
#include <filesystem>
#include <cassert>

namespace mp{

    //GotW #29: реализуем case-insensitive строки
    struct ci_char_traits: public std::char_traits<char>{
        static bool eq(char l, char r){ return toupper(l) == toupper(r); }
        static bool ne(char l, char r){ return toupper(l) != toupper(r); }
        static bool lt(char l, char r){ return toupper(l) < toupper(r); }
        static int compare(const char* l, const char* r, std::size_t n){
            for(int i = 0; i < n; i++){
               if(toupper(l[i]) < toupper(r[i])) return -1;
               else if(toupper(l[i]) > toupper(r[i])) return 1;
            }
            return 0;
        }
        static const char* find(const char* s, int n, char a){
            while(n-- > 0 && toupper(*s) != toupper(a))
                ++s;
            return s;
        }
    };

    using ci_string = std::basic_string<char, ci_char_traits>;

    void FTPConnection::startActing() {
        _parent->enqueueConnection(_parent->fd(),
                          std::make_shared<FTPConnection>(
                                  _parent,
                                  std::move(_root),
                                  std::move(_fileSystem)
                          )
                          );
        _state = std::make_unique<FTPConnectionStateNotLoggedIn>(this);
        _ring->async_write(
                _fd,
                std::make_shared<std::string>("220-Connection Established\r\n220-Note that this server accepts only\r\n220 anonymous access mode.\r\n"s),
                96,
                defaultAsyncOpHandler
        );
    }

    void FTPConnection::processCommand(std::size_t commandLen) {
        if(commandLen < 0)
            stop();
        ci_string controlSeq;
        std::string commandField;
        if(_command->find(' ') < _command->find("\r\n")) {
                controlSeq = _command->substr(0, _command->find(' ')).c_str(); // NOLINT(readability-redundant-string-cstr)
                commandField = _command->substr(_command->find(' ') + 1,
                                                        _command->find("\r\n") - _command->find(' ') - 1);
        } else {
            controlSeq = _command->substr(0, _command->find("\r\n")).c_str(); // NOLINT(readability-redundant-string-cstr)
            commandField = "";

        }
        _command->erase(0, _command->find("\r\n") + 2);
        if(controlSeq == "USER"){
            _state->user(commandField);
        } else if (controlSeq == "CWD"){
            _state->cwd(commandField);
        } else if (controlSeq == "CDUP"){
            _state->cdup();
        } else if (controlSeq == "QUIT"){
            _state->quit();
        } else if (controlSeq == "TYPE"){
            _state->type(commandField);
        } else if (controlSeq == "STRU"){
            _state->stru(commandField);
        } else if (controlSeq == "MODE"){
            _state->mode(commandField);
        } else if (controlSeq == "RETR"){
            _state->retr(commandField);
        } else if (controlSeq == "STOR"){
            _state->stor(commandField);
        } else if (controlSeq == "PWD"){
            _state->pwd();
        } else if (controlSeq == "LIST"){
            _state->list(commandField);
        } else if (controlSeq == "NOOP"){
            _state->noop();
        } else if (controlSeq == "PASV"){
            _state->pasv();
        } else {
            _ring->async_write(_fd, std::make_shared<std::string>("500 Incorrect Command\r\n"s), 23, defaultAsyncOpHandler);
        }
    }

    void FTPConnection::makePasv() {
        if(_pasvFD >= 0)
            close(_pasvFD);
         _pasvFD = socket(_localAddr.sin_family, SOCK_STREAM, 0);
         struct sockaddr_in _pasvAddr{};
         socklen_t _pasvAddrLen;
        _pasvAddr.sin_family = _localAddr.sin_family;
        _pasvAddr.sin_addr = _localAddr.sin_addr;
        _pasvAddr.sin_port = 0;
        _pasvAddrLen = sizeof(_pasvAddr);
        bind(_pasvFD, reinterpret_cast<sockaddr*>(&_pasvAddr), _pasvAddrLen);
        listen(_pasvFD, 20);
        getsockname(_pasvFD, reinterpret_cast<sockaddr*>(&_pasvAddr), &_pasvAddrLen);

        std::uint32_t ip = ntohl(_pasvAddr.sin_addr.s_addr);
        std::uint16_t port = ntohs(_pasvAddr.sin_port);
        std::stringstream ss;
        ss << "227 Entering Passive Mode ("
           << ((ip & 0xFF000000) >> 24) << ","
           << ((ip & 0xFF0000) >> 16) << ","
           << ((ip & 0xFF00) >> 8) << ","
           << (ip & 0xFF) << ","
           << ((port & 0xFF00) >> 8) << ","
           << (port & 0xFF) << ").\r\n";
        auto connection = std::make_shared<FTPConnectionDataSender>(
                this,
                std::move(_fileSystem),
                [](){}
        );
        _ring->async_write(
                _fd,
                std::make_shared<std::string>(ss.str()),
                ss.str().size(),
                [this, connection](int res) mutable {
                    enqueueConnection(
                            _pasvFD,
                            std::move(connection)
                    );
                    defaultAsyncOpHandler(res);
                }
        );
        _currentPasvChild = connection;
    }

    void FTPConnection::postDataSendTask(std::filesystem::path&& path, ConnectionMode mode,
                                         std::function<void()>&& dataTransferEndCallback) {
        if(_currentPasvChild)
            _currentPasvChild->command(std::move(path), mode, std::move(dataTransferEndCallback));
    }


    void FTPConnectionDataSender::command(path &&pathToFile, ConnectionMode mode,
                                          std::function<void()> &&dataTransmissionEndCallback) {
        _pathToFile = pathToFile;
        _mode = mode;
        _dataTransmissionEndCallback = dataTransmissionEndCallback;
        _bytesRead = 0;

        if(_mode == ConnectionMode::sender)
            _fileFd = _fileSystem->open(_pathToFile, FTPFileSystem::OpenMode::readonly);
        else if (_mode == ConnectionMode::receiver)
            _fileFd = _fileSystem->open(_pathToFile, FTPFileSystem::OpenMode::writeonly);
        else {
            _fileStruct = popen(("ls -l " + _pathToFile.string()).c_str(), "r");
            _fileFd = _fileStruct->_fileno;
        }
        //Now we have opened the requested file and need to start the connection session
        continue_transmission(0);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"
    FTPConnectionDataSender::FTPConnectionDataSender(FTPConnectionBase* parent,
                                                     std::shared_ptr<FTPFileSystem> &&fileSystem,
                                                     std::function<void(void)> &&waitingForConnectionCallback):
            FTPConnectionBase(parent,
                              std::move(waitingForConnectionCallback)
            ),
            _fileSystem(fileSystem),
            _buffer(std::make_shared<std::string>()){
        continue_transmission = [this](std::int64_t res){
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
                            _buffer->resize(res);
                            int pos = 0;
                            while((pos = _buffer->find('\n', pos + 2)) != std::string::npos)
                                _buffer->replace(pos, 1, "\r\n");
                            _bytesRead += res;
                            _ring->async_write_some(_fd, std::move(_buffer), continue_transmission);
                        } else {
                            //read from file failed - eof reached
                            _fileSystem->close(_fileFd);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    }, _bytesRead);
                } else if(_mode == ConnectionMode::receiver) {
                    _ring->async_read_some(_fd, std::move(_buffer), [this](int res){
                        if(res > 0){
                            //read from socket successful
                            _buffer->resize(res);
                            int pos = 0;
                            while((pos = _buffer->find("\r\n", pos + 1)) != std::string::npos)
                                _buffer->replace(pos, 2, "\n");
                            _bytesRead += _buffer->size();
                            _ring->async_write_some(_fileFd, std::move(_buffer), continue_transmission, _bytesRead - _buffer->size());
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
                            _buffer->resize(res);
                            int pos = 0;
                            while((pos = _buffer->find('\n', pos + 2)) != std::string::npos)
                                _buffer->replace(pos, 1, "\r\n");
                            _bytesRead += res;
                            _ring->async_write_some(_fd, std::move(_buffer), continue_transmission);
                        } else {
                            //read from file failed - eof reached
                            pclose(_fileStruct);
                            _dataTransmissionEndCallback();
                            stop();
                        }
                    }, _bytesRead);
                }
            }
        };
    }
#pragma clang diagnostic pop
}