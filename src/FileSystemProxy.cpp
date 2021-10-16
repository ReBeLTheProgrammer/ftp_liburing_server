#include <FileSystemProxy.h>

namespace ftp {

    int FileSystemProxy::open(const std::filesystem::path &relativePath, FileSystemProxy::OpenMode mode) {
        assert(relativePath.has_filename());
        auto lk = std::lock_guard(_filesystemMutex);
        if (!_fileTable.contains(relativePath))
            updateFileTable(relativePath);
        if (mode == OpenMode::readonly) {
            if (!_fileTable[relativePath].empty()) {
                auto file = _fileTable[relativePath].back();
                int fd = ::open((file->_truePath).c_str(), O_RDONLY);
                _fdTable.emplace(std::make_pair(fd, file));
                return fd;
            } else return -1;
        } else {
            //Opening file for writing purposes means updating the file stored by FTP implementation
            // and requires creation of a new file in order to save access to previous versions
            // for the clients who are now reading them.

            //We need to do some magic about the relative path here in order to name the file uniquely
            std::stringstream ss;
            ss << std::chrono::system_clock::now().time_since_epoch().count();
            path filePath = _root / ".tmp" / relativePath / ss.str();
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

    void FileSystemProxy::close(int fd) {
        auto lk = std::lock_guard(_filesystemMutex);
        if (_fdTable.contains(fd)) {
            ::close(fd);
            auto filePointer = _fdTable[fd];
            _fdTable.erase(fd);
            if (_fileTable[filePointer->_keyPath].back() != filePointer && filePointer.use_count() == 2) {
                //This means there are only this function and the fileTable are left to own the file and
                //the file itself is outdated, so we can safely remove it from our table (only if it is in .tmp dir)
                _fileTable[filePointer->_keyPath].erase(
                        std::find(
                                _fileTable[filePointer->_keyPath].begin(),
                                _fileTable[filePointer->_keyPath].end(),
                                filePointer));
                remove(filePointer->_truePath.c_str());
            }
        } else if (_fdsBeingEdited.contains(fd)) {
            //This means we're trying to close a file that was opened in writeonly mode
            //So we need to close the fd and transfer this file to the fileTable.
            ::close(fd);
            auto file = _fdsBeingEdited[fd];
            for (auto it = _fileTable[file->_keyPath].begin(); it != _fileTable[file->_keyPath].end(); it++)
                if (it->unique() && (*it)->_truePath != _root / file->_keyPath) {
                    remove((*it)->_truePath.c_str());
                    _fileTable[file->_keyPath].erase(it);
                    it--;
                }
            _fileTable[file->_keyPath].push_back(file);
            _fdsBeingEdited.erase(fd);
            _filesBeingEdited.erase(file->_keyPath);
        }
    }

    void FileSystemProxy::loadFileTable(const std::filesystem::path &relPath) {

        for (auto &item: directory_iterator(_root / relPath)) {
            if (item.is_directory() && item.path().filename() != ".tmp")
                loadFileTable(relPath / item.path().filename());
            else if (item.is_regular_file())
                updateFileTable(relPath / item.path().filename());
        }
    }

    void FileSystemProxy::updateFileTable(const std::filesystem::path &relativePath) {
        assert(relativePath.has_filename());
        assert(!relativePath.has_root_path());
        auto lk = std::lock_guard(_filesystemMutex);
        auto originalFilePath = _root / relativePath,
                tmpFileDir = _root / ".tmp" / relativePath;

        if (!is_directory(tmpFileDir) && exists(tmpFileDir))
            throw std::runtime_error("FTP File tree is corrupt");

        std::vector<directory_entry> files = {directory_entry(originalFilePath)};
        if (exists(tmpFileDir))
            for (auto &tmpFile: directory_iterator(tmpFileDir))
                files.push_back(tmpFile);
        std::sort(files.begin(), files.end(), [](auto &lhs, auto &rhs) {
            return lhs.last_write_time() < rhs.last_write_time();
        });

        for (auto &file: files) {
            auto fileEntryPtr = std::make_shared<FTPFileEntry>();
            fileEntryPtr->_keyPath = relativePath;
            fileEntryPtr->_truePath = file.path();
            if (!_filesBeingEdited.contains(file.path())) {
                if (file.path() != _root / relativePath && file != files.back())
                    remove(file.path().c_str());
                else
                    _fileTable[relativePath].push_back(fileEntryPtr);
            }
        }
    }

}
