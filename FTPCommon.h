#ifndef URING_TCP_SERVER_FTPCOMMON_H
#define URING_TCP_SERVER_FTPCOMMON_H

#include <string>
#include <map>
#include <queue>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <filesystem>
#include <cassert>
#include <mutex>
#include <functional>
#include <iostream>
#include "async_uring.h"

extern std::mutex m;

namespace mp {

    using namespace std::filesystem;

class FTPFileSystem {
    public:
        enum class OpenMode{
            readonly,
            writeonly
        };

        explicit FTPFileSystem(const path& path){
            assert(path.has_filename());
            assert(path.has_root_path());
            assert(exists(path));
            _root = path;
            loadFileTable();
        }

        /**
         * open - opens the latest file in selected mode
         * @param relativePath - path to the file inside the FTP sandbox. Should not end with /
         * @return ind fd - file descriptor of the opened file.
         */
        int open(const path& relativePath, OpenMode mode){
            assert(relativePath.has_filename());
            auto lk = std::lock_guard(_filesystemMutex);
            if(!_fileTable.contains(relativePath))
                updateFileTable(relativePath);
            if(mode == OpenMode::readonly) {
                if (!_fileTable[relativePath].empty()) {
                    auto file = _fileTable[relativePath].back();
                    int fd = ::open((file->_truePath).c_str(), O_RDONLY);
                    _fdTable.emplace(std::make_pair(fd, file));
                    return fd;
                }
                else return -1;
            } else {
                //Opening file for writing purposes means updating the file stored by FTP implementation
                // and requires creation of a new file in order to save access to previous versions
                // for the clients who are now reading them.

                //We need to do some magic about the relative path here in order to name the file uniquely
                std::stringstream ss;
                ss << std::chrono::system_clock::now().time_since_epoch().count();
                path filePath = _root/".tmp"/relativePath/ss.str();
                create_directories(filePath.parent_path());
                int fd = ::open(filePath.c_str(), O_WRONLY | O_CREAT, 0666);
                auto file = std::make_shared<FTPFileEntry>();
                file->_keyPath = relativePath;
                file->_truePath = filePath;
                _fdsBeingEdited.emplace(std::make_pair(fd, file));
                _filesBeingEdited.emplace(std::make_pair(file->_keyPath, file));
                return fd;
            }
        }

        void close(int fd){
            auto lk = std::lock_guard(_filesystemMutex);
            if(_fdTable.contains(fd)){
                ::close(fd);
                auto filePointer = _fdTable[fd];
                _fdTable.erase(fd);
                if(_fileTable[filePointer->_keyPath].back() != filePointer && filePointer.use_count() == 2) {
                    //This means there are only this function and the fileTable are left to own the file and
                    //the file itself is outdated, so we can safely remove it from our table (only if it is in .tmp dir)
                    _fileTable[filePointer->_keyPath].erase(
                            std::find(
                                    _fileTable[filePointer->_keyPath].begin(),
                                    _fileTable[filePointer->_keyPath].end(),
                                    filePointer));
                    remove(filePointer->_truePath.c_str());
                }
            } else if(_fdsBeingEdited.contains(fd)){
                //This means we're trying to close a file that was opened in writeonly mode
                //So we need to close the fd and transfer this file to the fileTable.
                ::close(fd);
                auto file = _fdsBeingEdited[fd];
                for(auto it = _fileTable[file->_keyPath].begin(); it != _fileTable[file->_keyPath].end(); it++)
                    if(it->unique() && (*it)->_truePath != _root/file->_keyPath){
                        remove((*it)->_truePath.c_str());
                        _fileTable[file->_keyPath].erase(it);
                        it--;
                    }
                _fileTable[file->_keyPath].push_back(file);
                _fdsBeingEdited.erase(fd);
                _filesBeingEdited.erase(file->_keyPath);
            }
        }

        ~FTPFileSystem(){
            //Before the destruction of filesystem, it copies all the latest versions of files to their target
            //destinations, dropping the ones that are outdated.
            for(const auto& it: _fileTable){
                for(const auto& iter: it.second)
                    if(iter == it.second.back())
                        std::filesystem::rename(iter->_truePath, _root/iter->_keyPath);
                    else remove(iter->_truePath);
            }
        }

    private:

        struct FTPFileEntry {
            path _keyPath;
            path _truePath;
        };

        path _root;
        std::map<path, std::vector<std::shared_ptr<FTPFileEntry>>> _fileTable;
        std::map<int, std::shared_ptr<FTPFileEntry>> _fdTable;
        std::map<int, std::shared_ptr<FTPFileEntry>> _fdsBeingEdited;
        std::map<path, std::shared_ptr<FTPFileEntry>> _filesBeingEdited;
        std::mutex _filesystemMutex;

        void loadFileTable(const path& relPath = ""){

            for(auto& item: directory_iterator(_root/relPath)){
                if(item.is_directory() && item.path().filename() != ".tmp")
                    loadFileTable(relPath/item.path().filename());
                else if(item.is_regular_file())
                    updateFileTable(relPath/item.path().filename());
            }
        }

        /**
         * updateFileTable - private class member function that adds info about the files on disk
         * @param relativePath - FTP-root-relative path to file of interest, should not end with /
         */
        void updateFileTable(const path& relativePath){
            assert(relativePath.has_filename());
            assert(!relativePath.has_root_path());
            auto lk = std::lock_guard(_filesystemMutex);
            auto originalFilePath = _root/relativePath,
                tmpFileDir = _root/".tmp"/relativePath;

            if (!is_directory(tmpFileDir) && exists(tmpFileDir))
                throw std::runtime_error("FTP File tree is corrupt");

            std::vector<directory_entry> files = {directory_entry(originalFilePath)};
            if(exists(tmpFileDir))
                for(auto& tmpFile: directory_iterator(tmpFileDir))
                    files.push_back(tmpFile);
            std::sort(files.begin(), files.end(), [](auto& lhs, auto& rhs){
                return lhs.last_write_time() < rhs.last_write_time();
            });

            for(auto& file: files){
                auto fileEntryPtr = std::make_shared<FTPFileEntry>();
                fileEntryPtr->_keyPath = relativePath;
                fileEntryPtr->_truePath = file.path();
                if(!_filesBeingEdited.contains(file.path())) {
                    if (file.path() != _root / relativePath && file != files.back())
                        remove(file.path().c_str());
                    else
                        _fileTable[relativePath].push_back(fileEntryPtr);
                }
            }
        }
    };

    class FTPConnectionBase{
    public:
        using FTPConnectionQueueType = std::map<int, std::queue<std::shared_ptr<FTPConnectionBase>>>;
        FTPConnectionBase(int fd,
                          std::shared_ptr<std::mutex>&& childConnectionsMutex,
                          sockaddr_in localAddr,
                          std::shared_ptr<uring_wrapper>&& ring,
                          std::function<void(void)>&& waitingForConnectionCallback = [](){}
                          ):
        _fd(fd),
        _childConnectionsMutex(childConnectionsMutex),
        _parent(std::shared_ptr<FTPConnectionBase>()),
        _localAddr(localAddr),
        _waitingForConnectionCallback(waitingForConnectionCallback),
        _ring(ring){}

        FTPConnectionBase(std::shared_ptr<FTPConnectionBase>&& parent,
                          std::function<void(void)>&& waitingForConnectionCallback = [](){}
                          ):
                _fd(parent->_fd),
                _childConnectionsMutex(parent->_childConnectionsMutex),
                _parent(parent),
                _localAddr(parent->_localAddr),
                _waitingForConnectionCallback(waitingForConnectionCallback),
                _ring(parent->_ring) {}

        virtual void start() {
            _addrLen = sizeof(_remoteAddr);
            //Notify the parent that we are waiting for connection.
            _waitingForConnectionCallback();
            _ring->async_sock_accept(_fd, reinterpret_cast<sockaddr*>(&_remoteAddr), &_addrLen, 0, [this](int res){
                _fd = res;
                {
                    auto lk = std::lock_guard(m);
                    std::cout << "async_sock_accept() returned " << res << '\n';
                }
                if(_fd < 0){
                    //stop self
                    stop();
                } else{
                    //After all queue manipulations, we can finally start the protocol payload functioning.
                    startActing();
                }
            });
        }

        sockaddr_in localAddr() const {
            return _localAddr;
        }

        int fd() const noexcept{
            return _fd;
        }

        //Kills the connection with all child connections.
        virtual void stop() {
            auto lk = std::lock_guard(*_childConnectionsMutex);
            for(auto& child: _childConnections)
                child->stop();
            close(_fd);
            if(_parent)
                _parent->acceptChildStop(this);
        }

        void enqueueConnection(int fd, std::shared_ptr<FTPConnectionBase>&& connection) {
            socklen_t addrLen = sizeof(sockaddr_in);
            getsockname(fd, reinterpret_cast<sockaddr*>(&_localAddr), &addrLen);
            connection->_fd = fd;
            {
                auto lk = std::lock_guard(*_childConnectionsMutex);
                _childConnections.push_back(connection);
            }
            connection->start();
        }

        virtual ~FTPConnectionBase(){
            close(_fd);
            auto lk = std::lock_guard(*_childConnectionsMutex);
            for(auto& child: _childConnections)
                child->stop();
        }

    protected:
        int _fd;
        sockaddr_in _localAddr, _remoteAddr;
        socklen_t _addrLen;
        std::vector<std::shared_ptr<FTPConnectionBase>> _childConnections;
        std::shared_ptr<std::mutex> _childConnectionsMutex;
        std::shared_ptr<FTPConnectionBase> _parent;
        std::function<void(void)> _waitingForConnectionCallback;
        std::shared_ptr<uring_wrapper> _ring;

        virtual void startActing() = 0;

        //To keep the children list up to date, we need to find and erase the closed child from the children list
        void acceptChildStop(FTPConnectionBase* child){
            //Adding thread-safety
            auto lk = std::lock_guard(*_childConnectionsMutex);
            _childConnections.erase(
                    std::find_if(
                            _childConnections.begin(),
                            _childConnections.end(),
                            [child](const auto& el){return el.get() == child;}));
        }

    };

    enum class FTPTransferMode: char{
        Stream = 'S'
    };
    enum class FTPRepresentationType: char{
        ASCII = 'A',
        NonPrint = 'N'
    };
    enum class FTPFileStructure: char{
        File = 'F',
        Record = 'R'
    };

}

#endif //URING_TCP_SERVER_FTPCOMMON_H
