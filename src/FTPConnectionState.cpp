#include <FTPConnectionState.h>
#include <FTPConnection.h>


namespace mp{

    /**
     * @brief This function processes the path from _arg given by user and returns the full path if it is root-safe. Transitions to .tmp directories are also prohibited.
     * @param pwd path to current directory of the server, starting and ending with /
     * @param arg path given by user
     * @throw std::runtime_error if the path is root-unsafe or the path contains transition to .tmp
     * @return root-safe root-relative path, starting with /
     */
    std::filesystem::path parsePath(const std::filesystem::path& pwd, const std::filesystem::path& arg){
        std::filesystem::path res;
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
            if(!res.empty() || !(token == "/"))
                res /= token;
        return res;
    }

    void FTPConnectionState::noop() const {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("200 Ok\r\n"s),
                8,
                _handledConnection->defaultAsyncOpHandler
        );
    }

    void FTPConnectionState::quit() {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("221 Bye\r\n"s),
                9,
                [this](int res){
                    _handledConnection->stop();
                }
                );
    }

    void FTPConnectionStateNotLoggedIn::user(const std::string& username) {
        if(username == "anonymous") {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("230 User Name OK\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(
                    std::make_unique<FTPConnectionStateLoggedIn>(_handledConnection));
        }
        else
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("530 User Name Incorrect\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void FTPConnectionStateLoggedIn::user(const std::string& username) {
        if(username != "anonymous") {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("530 User Name Incorrect\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(std::make_unique<FTPConnectionStateNotLoggedIn>(_handledConnection));
        } else {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("230 User Name OK\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(std::make_unique<FTPConnectionStateLoggedIn>(_handledConnection));
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
                    std::make_shared<std::string>("550 Illegal path\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("550 File does not exist\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(!std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("550 Specified path is not a directory\r\n"s),
                    39,
                    _handledConnection->defaultAsyncOpHandler
            );
        else {
            _handledConnection->pwd() = path;
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Directory changed\r\n"s),
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
                    std::make_shared<std::string>("200 Directory changed\r\n"s),
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        } else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("550 Path not found\r\n"s),
                    20,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void FTPConnectionStateLoggedIn::port(const std::string& hostPort){
        _handledConnection->ring()->async_write(_handledConnection->fd(),
                                                std::make_shared<std::string>("500 Command unavailable\r\n"s),
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
                    std::make_shared<std::string>("200 Type changed\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
        else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Invalid/Unsupported TYPE parameter\r\n"s),
                    40,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void FTPConnectionStateLoggedIn::stru(const std::string& structureCode) {
        if(structureCode == std::string{static_cast<char>(FTPFileStructure::File)}){
            _handledConnection->setStructure(FTPFileStructure::File);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Structure changed\r\n"s),
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        } else if(structureCode == std::string{static_cast<char>(FTPFileStructure::Record)}){
            _handledConnection->setStructure(FTPFileStructure::Record);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Structure changed\r\n"s),
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        } else{
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Invalid/Unsupported STRUcture parameter\r\n"s),
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
                    std::make_shared<std::string>("200 Mode changed\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
        else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Invalid/Unsupported MODE parameter\r\n"s),
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
                    std::make_shared<std::string>("501 Illegal path\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 File does not exist\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Specified path is a directory\r\n"s),
                    35,
                    _handledConnection->defaultAsyncOpHandler
            );
        else{
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("150 Opened data connection\r\n"s),
                    28,
                    [this, path](int res) mutable {
                        _handledConnection->postDataSendTask(std::move(path), ConnectionMode::sender, [this](){
                            _handledConnection->ring()->async_write(
                                    _handledConnection->fd(),
                                    std::make_shared<std::string>("250 Operation successful\r\n"),
                                    26,
                                    _handledConnection->defaultAsyncOpHandler
                            );
                        });}
            );
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
                    std::make_shared<std::string>("501 Illegal path\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }

        if(!std::filesystem::exists(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 File does not exist\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
        else if(std::filesystem::is_directory(fullPath))
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Specified path is a directory\r\n"s),
                    35,
                    _handledConnection->defaultAsyncOpHandler
            );
        else{
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("150 Opened data connection\r\n"s),
                    28,
                    [this, path](int res) mutable {
                        _handledConnection->postDataSendTask(std::move(path), ConnectionMode::receiver, [this](){
                            _handledConnection->ring()->async_write(
                                    _handledConnection->fd(),
                                    std::make_shared<std::string>("250 Operation successful\r\n"),
                                    26,
                                    _handledConnection->defaultAsyncOpHandler
                            );
                        });}
            );
        }
    }

    void FTPConnectionStateLoggedIn::pwd() const {
        std::filesystem::path path = "/"/_handledConnection->pwd();
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("200 "s + path.string() + "\r\n"s),
                6 + path.string().size(),
                _handledConnection->defaultAsyncOpHandler
        );
    }

    void FTPConnectionStateLoggedIn::list(std::filesystem::path path) const {
        //Проходом по directory_iterator выбираем все файлы и отправляем их filenames
        try {
            path = parsePath(_handledConnection->pwd(), path);
        } catch (const std::exception &e) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Illegal path\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            return;
        }
        if(std::filesystem::is_directory(_handledConnection->root()/path)) {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("150 Opened data connection\r\n"s),
                    28,
                    [this, path](int res) mutable {
                        _handledConnection->postDataSendTask(std::move(path), ConnectionMode::lister, [this](){
                            _handledConnection->ring()->async_write(
                                    _handledConnection->fd(),
                                    std::make_shared<std::string>("250 Operation successful\r\n"),
                                    26,
                                    _handledConnection->defaultAsyncOpHandler
                            );
                        });}
            );

        }
        else
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 specified path is not a directory\r\n"s),
                    39,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void FTPConnectionStateLoggedIn::pasv() {
        _handledConnection->makePasv();
    }

}