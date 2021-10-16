#ifndef URING_TCP_SERVER_ASYNCURING_H
#define URING_TCP_SERVER_ASYNCURING_H

#include <liburing.h>
#include <string>
#include <cstdint>
#include <system_error>
#include <utility>
#include <span>
#include <functional>
#include <memory>
#include <arpa/inet.h>
#include <boost/intrusive/list.hpp>
#include <mutex>

namespace ftp{

    using Callback = std::function<void(std::int64_t)>;
    using Predicate = std::function<std::ptrdiff_t(const std::string&)>;

    class AsyncUring{
    public:
        enum class UringMode{
            interrupted = 0,
            io_polled = IORING_SETUP_IOPOLL,
            sq_polled = IORING_SETUP_SQPOLL
        };
        explicit AsyncUring(int size, UringMode mode = UringMode::interrupted){
            assert(
                    size == 1UL ||
                    size == 1UL << 1 ||
                    size == 1UL << 2 ||
                    size == 1UL << 3 ||
                    size == 1UL << 4 ||
                    size == 1UL << 5 ||
                    size == 1UL << 6 ||
                    size == 1UL << 7 ||
                    size == 1UL << 8 ||
                    size == 1UL << 9 ||
                    size == 1UL << 10 ||
                    size == 1UL << 11 ||
                    size == 1UL << 12
            );
            int res = io_uring_queue_init(size, &ring, int(mode));
            if(res < 0) {
                throw std::system_error(errno, std::system_category(), "AsyncUring()");
            }
        }

        void async_read_some(int fd, std::shared_ptr<std::string>&& data, Callback cb, int offset = 0);
        void async_read_some(int fd, std::span<std::byte> data, std::shared_ptr<std::string>&& dataToKeep, Callback cb, int offset = 0);
        void async_write_some(int fd, std::shared_ptr<std::string>&& data, Callback cb, int offset = 0);
        void async_write_some(int fd, std::span<const std::byte> data, std::shared_ptr<std::string>&& dataToKeep, Callback cb, int offset = 0);
        void async_read(int fd, std::shared_ptr<std::string>&& data, std::size_t len, Callback cb, int offset = 0);
        void async_write(int fd, std::shared_ptr<std::string>&& data, std::size_t len, Callback cb, int offset = 0);
        void async_read_until(int fd, std::shared_ptr<std::string>&& data, const std::string& delim, Callback cb, int offset = 0);
        void async_read_until(int fd, std::shared_ptr<std::string>&& data, Predicate pred, Callback cb, int offset = 0);
        void async_sock_accept(int fd, sockaddr* addr, socklen_t* len, int flags, Callback cb);
        void async_sock_connect(int fd, sockaddr* addr, socklen_t len, Callback cb);
        std::function<void()> check_act();

        ~AsyncUring(){
            io_uring_queue_exit(&ring);
            active_callbacks.clear_and_dispose(std::default_delete<intrusive_callback>());
        }

    private:
        io_uring ring{};
        std::mutex _taskPostMutex;

        class intrusive_callback: public boost::intrusive::list_base_hook<> {
            public:
                explicit intrusive_callback(Callback cb, std::shared_ptr<std::string>&& i_data): cb_(std::move(cb)), i_data_(i_data){}
                //To avoid memory leak, we have to ensure that the data we interact with is still present.
                std::shared_ptr<std::string> i_data_;
                Callback cb_;
            };

        boost::intrusive::list<intrusive_callback> active_callbacks;
    };

}


#endif //URING_TCP_SERVER_ASYNCURING_H
