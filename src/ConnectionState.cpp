#include <ConnectionState.h>
#include <ControlConnection.h>


namespace ftp{

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

    void ControlConnectionState::noop() const {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("200 Ok\r\n"s),
                8,
                _handledConnection->defaultAsyncOpHandler
        );
    }

    void ControlConnectionState::quit() {
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("221 Bye\r\n"s),
                9,
                [this](int res){
                    _handledConnection->stop();
                }
                );
    }

    void ControlConnectionStateNotLoggedIn::user(const std::string& username) {
        if(username == "anonymous") {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("230 User Name OK\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(
                    std::make_unique<ControlConnectionStateLoggedIn>(_handledConnection));
        }
        else
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("530 User Name Incorrect\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void ControlConnectionStateLoggedIn::user(const std::string& username) {
        if(username != "anonymous") {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("530 User Name Incorrect\r\n"s),
                    25,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(std::make_unique<ControlConnectionStateNotLoggedIn>(_handledConnection));
        } else {
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("230 User Name OK\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
            _handledConnection->switchState(std::make_unique<ControlConnectionStateLoggedIn>(_handledConnection));
        }
    }

    void ControlConnectionStateLoggedIn::cwd(std::string path) {
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

    void ControlConnectionStateLoggedIn::cdup() {
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

    void ControlConnectionStateLoggedIn::port(const std::string& hostPort){
        _handledConnection->ring()->async_write(_handledConnection->fd(),
                                                std::make_shared<std::string>("500 Command unavailable\r\n"s),
                                                25,
                                                _handledConnection->defaultAsyncOpHandler);
    }

    void ControlConnectionStateLoggedIn::type(const std::string& typeCode) {
        if(
                typeCode == std::string{static_cast<char>(RepresentationType::ASCII)} ||
                typeCode == std::string{static_cast<char>(RepresentationType::ASCII)}
                            + static_cast<char>(RepresentationType::NonPrint)) {
            _handledConnection->setRepresentationType(RepresentationType::ASCII);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Type changed\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
        }
        else if(typeCode == std::string{static_cast<char>(RepresentationType::Image)}){
            _handledConnection->setRepresentationType(RepresentationType::Image);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Type changed\r\n"s),
                    18,
                    _handledConnection->defaultAsyncOpHandler
            );
        }
        else _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("501 Invalid/Unsupported TYPE parameter\r\n"s),
                    40,
                    _handledConnection->defaultAsyncOpHandler
            );
    }

    void ControlConnectionStateLoggedIn::stru(const std::string& structureCode) {
        if(structureCode == std::string{static_cast<char>(FileStructure::File)}){
            _handledConnection->setStructure(FileStructure::File);
            _handledConnection->ring()->async_write(
                    _handledConnection->fd(),
                    std::make_shared<std::string>("200 Structure changed\r\n"s),
                    23,
                    _handledConnection->defaultAsyncOpHandler
            );
        } else if(structureCode == std::string{static_cast<char>(FileStructure::Record)}){
            _handledConnection->setStructure(FileStructure::Record);
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

    void ControlConnectionStateLoggedIn::mode(const std::string& modeCode) {
        if(modeCode == std::string{static_cast<char>(TransferMode::Stream)})
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

    void ControlConnectionStateLoggedIn::retr(std::filesystem::path path) {
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
                        _handledConnection->postDataSendTask(std::move(path), DataConnectionMode::sender, [this](){
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

    void ControlConnectionStateLoggedIn::stor(std::filesystem::path path) {
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
                        _handledConnection->postDataSendTask(std::move(path), DataConnectionMode::receiver, [this](){
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

    void ControlConnectionStateLoggedIn::pwd() const {
        std::filesystem::path path = "/"/_handledConnection->pwd();
        _handledConnection->ring()->async_write(
                _handledConnection->fd(),
                std::make_shared<std::string>("200 "s + path.string() + "\r\n"s),
                6 + path.string().size(),
                _handledConnection->defaultAsyncOpHandler
        );
    }

    void ControlConnectionStateLoggedIn::list(std::filesystem::path path) const {
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
                        _handledConnection->postDataSendTask(std::move(path), DataConnectionMode::lister, [this](){
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

    void ControlConnectionStateLoggedIn::pasv() {
        _handledConnection->makePasv();
    }

}