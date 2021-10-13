//
// Created by therbl on 10/13/21.
//

#ifndef URING_TCP_SERVER_FTPFILESYSTEM_H
#define URING_TCP_SERVER_FTPFILESYSTEM_H

#include <string>
#include <map>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <cassert>
#include <mutex>
#include <vector>

namespace mp {

    using namespace std::filesystem;

    class FTPFileSystem {
    public:
        enum class OpenMode {
            readonly,
            writeonly
        };

        explicit FTPFileSystem(const path &path) {
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
        int open(const path &relativePath, OpenMode mode);

        void close(int fd);

        ~FTPFileSystem() {
            //Before the destruction of filesystem, it copies all the latest versions of files to their target
            //destinations, dropping the ones that are outdated.
            for (const auto &it: _fileTable) {
                for (const auto &iter: it.second)
                    if (iter == it.second.back())
                        std::filesystem::rename(iter->_truePath, _root / iter->_keyPath);
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

        void loadFileTable(const path &relPath = "");

        /**
         * updateFileTable - private class member function that adds info about the files on disk
         * @param relativePath - FTP-root-relative path to file of interest, should not end with /
         */
        void updateFileTable(const path &relativePath);
    };

}

#endif //URING_TCP_SERVER_FTPFILESYSTEM_H
