#include "FTPConnection.h"

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

    /**
     * @brief This function processes the path from _arg given by user and returns the full path if it is root-safe. Transitions to .tmp directories are also prohibited.
     * @param pwd path to current directory of the server, starting and ending with /
     * @param arg path given by user
     * @throw std::runtime_error if the path is root-unsafe or the path contains transition to .tmp
     * @return root-safe root-relative path, starting with /
     */
    std::filesystem::path parsePath(const std::filesystem::path& pwd, const std::filesystem::path& arg){
        std::filesystem::path res;
        assert(!pwd.has_root_path());
        assert(pwd.has_filename());
        if(arg.has_root_path())
            res = arg;
        else
            res = pwd/arg;

        std::vector<std::filesystem::path> tokens;
        for(const auto& token: res){
            //If we meet .., try to simulate a root-safe cdup on vector. If fails, throw an exception.
            if(token == ".."){
                if(tokens.empty())
                    throw std::runtime_error("The given path is potentially upper than root.");
                else tokens.pop_back();
            } else if(token == ".tmp")
                throw std::runtime_error("Transition to .tmp detected.");
            //Otherwise if . not met, push a new directory to the vector.
            else if(token != ".")
                tokens.push_back(token);
        }
        res.clear();
        for(const auto& token: tokens)
            res /= token;
        return res;
    }

    void FTPConnection::startActing() {
        _state = std::make_unique<FTPConnectionStateNotLoggedIn>(std::shared_ptr<FTPConnection>(reinterpret_cast<FTPConnection*>(this)));
        _ring->async_write(
                _fd,
                "220-Connection Established\r\n"
                "220-Note that this server accepts only\r\n"
                "220 anonymous access mode.\r\n"s,
                96,
                defaultAsyncOpHandler
        );
    }

    void FTPConnection::processCommand(std::size_t commandLen) {
        if(commandLen < 0)
            stop();
        ci_string controlSeq = _command.substr(0, _command.find(' ')).c_str(); // NOLINT(readability-redundant-string-cstr)
        std::string commandField = _command.substr(_command.find(' ') + 1, _command.find("\r\n"));
        _command.erase(0, _command.find("\r\n") + 2);
        if(controlSeq == "USER"){
            _state->user(commandField);
        } else if (controlSeq == "CWD"){
            _state->cwd(commandField);
        } else if (controlSeq == "CDUP"){
            _state->cdup();
        } else if (controlSeq == "QUIT"){
            _state->quit();
        } else if (controlSeq == "PORT"){
            _state->port(commandField);
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
        } else {
            _ring->async_write(_fd, "500 Incorrect Command\r\n"s, 25, defaultAsyncOpHandler);
        }
    }

    void FTPConnectionState::noop() const {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                "200 Ok\r\n"s,
                8,
                _handledConnection->defaultAsyncOpHandler
                );
    }

    void FTPConnectionStateNotLoggedIn::user(const std::string& username) {
        if(username == "anonymous")
            _handledConnection->switchState(std::make_unique<FTPConnectionStateLoggedIn>(std::move(_handledConnection)));
        else
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "530 User Name Incorrect\r\n"s,
                    25,
                    _handledConnection->defaultAsyncOpHandler
                    );
    }

    void FTPConnectionStateLoggedIn::user(const std::string& username) {
        if(username != "anonymous") {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "530 User Name Incorrect\r\n"s,
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(std::make_unique<FTPConnectionStateNotLoggedIn>(std::move(_handledConnection)));
        }
    }

    void FTPConnectionStateLoggedIn::cwd(std::string path) {
        std::filesystem::path fullPath;
        try {
            path = parsePath(_handledConnection->pwd(), path);
            fullPath = _handledConnection->root()/path;
        } catch (const std::exception &e) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "550 Illegal path\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "550 File does not exist\r\n"s,
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(!std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "550 Specified path is not a directory\r\n"s,
                    39,
                    _handledConnection->defaultAsyncOpHandler
            );
        else {
            _handledConnection->pwd() = path;
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Directory changed\r\n"s,
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        }
    }
    
    void FTPConnectionStateLoggedIn::cdup() {
        auto& pwd = _handledConnection->pwd();
        if(!pwd.empty()) {
            pwd = pwd.parent_path();
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Directory changed\r\n"s,
                    23,
                    _handledConnection->defaultAsyncOpHandler
                    );
        } else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "550 Path not found\r\n"s,
                    20,
                    _handledConnection->defaultAsyncOpHandler
                );
    }
    
    void FTPConnectionStateLoggedIn::quit() {
        _handledConnection->stop();
    }
    
    void FTPConnectionStateLoggedIn::port(const std::string& hostPort){
        _handledConnection->ring()->async_write(_handledConnection->fd(),
                                                "500 Command unavailable\r\n"s,
                                                25,
                                                _handledConnection->defaultAsyncOpHandler);
    }
    
    void FTPConnectionStateLoggedIn::type(const std::string& typeCode) {
        if(
                typeCode == std::string{static_cast<char>(FTPRepresentationType::ASCII)} ||
                typeCode == std::string{static_cast<char>(FTPRepresentationType::ASCII)}
                + static_cast<char>(FTPRepresentationType::NonPrint)
            )
            //Do nothing and print
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Type changed\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
                    );
        else _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                "501 Invalid/Unsupported TYPE parameter\r\n"s,
                40,
                _handledConnection->defaultAsyncOpHandler
                );
    }
    
    void FTPConnectionStateLoggedIn::stru(const std::string& structureCode) {
        if(structureCode == std::string{static_cast<char>(FTPFileStructure::File)}){
            _handledConnection->setStructure(FTPFileStructure::File);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Structure changed\r\n"s,
                    23,
                    _handledConnection->defaultAsyncOpHandler
                    );
        } else if(structureCode == std::string{static_cast<char>(FTPFileStructure::Record)}){
            _handledConnection->setStructure(FTPFileStructure::Record);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Structure changed\r\n"s,
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        } else{
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Invalid/Unsupported STRUcture parameter\r\n"s,
                    45,
                    _handledConnection->defaultAsyncOpHandler
            );
        }
    }
    
    void FTPConnectionStateLoggedIn::mode(const std::string& modeCode) {
        if(modeCode == std::string{static_cast<char>(FTPTransferMode::Stream)})
            //Do nothing and write
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "200 Mode changed\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
        else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Invalid/Unsupported MODE parameter\r\n"s,
                    40,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void FTPConnectionStateLoggedIn::retr(std::filesystem::path path) {
        //Retrieve operation is allowed only on files.
        std::string fullPath;
        try {
            path = parsePath(_handledConnection->pwd(), path);
            fullPath = _handledConnection->root()/path;
        } catch (const std::exception &e) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Illegal path\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 File does not exist\r\n"s,
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Specified path is a directory\r\n"s,
                    35,
                    _handledConnection->defaultAsyncOpHandler
            );
        else{
            int fd = _handledConnection->fileSystem()->open(path, FTPFileSystem::OpenMode::writeonly);
            if(fd == -1)
                _handledConnection->ring()->async_write(
                        _handledConnection->fd(),
                        "550 Error opening the requested file\r\n"s,
                        38,
                        _handledConnection->defaultAsyncOpHandler
                );
            else{
                _handledConnection->enqueueConnection(
                        _handledConnection->port(),
                        std::make_shared<FTPConnectionDataSender>(
                                std::shared_ptr<FTPConnectionBase>(reinterpret_cast<FTPConnectionBase*>(this)),
                                std::move(_handledConnection->fileSystem()),
                                std::move(path),
                                FTPConnectionDataSender::ConnectionMode::sender,
                                [this](){
                                    _handledConnection->ring()->async_write(
                                            _handledConnection->fd(),
                                            "226 File Transfer Successful\r\n"s,
                                            30,
                                            _handledConnection->defaultAsyncOpHandler
                                    );
                                },
                                [this](){
                                    _handledConnection->ring()->async_write(
                                            _handledConnection->fd(),
                                            "150 Opened data connection\r\n"s,
                                            28,
                                            [](std::int64_t fd){}
                                    );
                                }
                        )
                );
            }
        }
    }
    
    void FTPConnectionStateLoggedIn::stor(std::filesystem::path path) {
        //Stor operation is allowed only on existing files.
        std::string fullPath;
        try {
            path = parsePath(_handledConnection->pwd(), path);
            fullPath = _handledConnection->root()/path;
        } catch (const std::exception &e) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Illegal path\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 File does not exist\r\n"s,
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Specified path is a directory\r\n"s,
                    35,
                    _handledConnection->defaultAsyncOpHandler
            );
        else{
            int fd = _handledConnection->fileSystem()->open(path, FTPFileSystem::OpenMode::readonly);
            if(fd == -1)
                _handledConnection->ring()->async_write(
                        _handledConnection->fd(),
                        "450 Error opening the requested file\r\n"s,
                        38,
                        _handledConnection->defaultAsyncOpHandler
                );
            else{
                _handledConnection->enqueueConnection(
                        _handledConnection->port(),
                        std::make_shared<FTPConnectionDataSender>(
                                std::shared_ptr<FTPConnectionBase>(reinterpret_cast<FTPConnectionBase*>(this)),
                                std::move(_handledConnection->fileSystem()),
                                std::move(path),
                                FTPConnectionDataSender::ConnectionMode::receiver,
                                [this](){
                                    _handledConnection->ring()->async_write(
                                            _handledConnection->fd(),
                                            "226 File Transfer Successful\r\n"s,
                                            30,
                                            _handledConnection->defaultAsyncOpHandler
                                    );
                                },
                                [this](){
                                    _handledConnection->ring()->async_write(
                                            _handledConnection->fd(),
                                            "150 Opened data connection\r\n"s,
                                            28,
                                            [](std::int64_t fd){}
                                    );
                                }
                        )
                );
            }
        }
    }
    
    void FTPConnectionStateLoggedIn::pwd() const {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                "200 Current directory is "s + _handledConnection->pwd().string() + "\r\n"s,
                27 + _handledConnection->pwd().string().size(),
                _handledConnection->defaultAsyncOpHandler
                );
    }
    
    void FTPConnectionStateLoggedIn::list(std::filesystem::path path) const {
        //Проходом по directory_iterator выбираем все файлы и отправляем их filenames
        std::string listing;
        try {
            path = parsePath(_handledConnection->pwd(), path);
        } catch (const std::exception &e) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 Illegal path\r\n"s,
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }
        if(std::filesystem::is_directory(_handledConnection->root()/path)) {
            for (auto &item: std::filesystem::directory_iterator(_handledConnection->root() / path)) {
                listing += "250-" + item.path().filename().string() + "\r\n";
            }
            listing += "250 Operation successful\r\n";
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    listing,
                    listing.size(),
                    _handledConnection->defaultAsyncOpHandler
            );
        }
        else
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    "501 specified path is not a directory\r\n"s,
                    39,
                    _handledConnection->defaultAsyncOpHandler
            );
    }
}