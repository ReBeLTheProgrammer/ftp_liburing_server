#ifndef URING_TCP_SERVER_ASYNC_URING_H
#define URING_TCP_SERVER_ASYNC_URING_H

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
#include <iostream>

extern std::mutex m;

namespace mp{

    using Callback = std::function<void(std::int64_t)>;
    using Predicate = std::function<std::ptrdiff_t(const std::string&)>;

    class uring_wrapper{
    public:
        enum class uring_mode{
            interrupted = 0,
            io_polled = IORING_SETUP_IOPOLL,
            sq_polled = IORING_SETUP_SQPOLL
        };
        explicit uring_wrapper(int size, uring_mode mode = uring_mode::interrupted){
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
                throw std::system_error(errno, std::system_category(), "uring_wrapper()");
            }
        }

        void async_read_some(int fd, std::shared_ptr<std::string>&& data, Callback cb, int offset = 0){
            static_assert(sizeof(decltype(*data->data())) == 1, "Buffer must have byte-sized elements.");
            async_read_some(fd, std::span<std::byte>({reinterpret_cast<std::byte*>(data->data()), data->size()}), std::move(data), std::move(cb), offset);
        }

        void async_read_some(int fd, std::span<std::byte> data, std::shared_ptr<std::string>&& dataToKeep, Callback cb, int offset = 0){
            auto lk = std::lock_guard(_taskPostMutex);
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            io_uring_prep_read(task, fd, data.data(), data.size(), offset);
            auto i_callback = new intrusive_callback(std::move(cb), std::move(dataToKeep));
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if(res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_readv()");
            }
            active_callbacks.push_back(*i_callback);
        }

        void async_write_some(int fd, std::shared_ptr<std::string>&& data, Callback cb, int offset = 0){
            static_assert(sizeof(decltype(*data->data())) == 1, "Buffer must have byte-sized elements.");
            async_write_some(fd, std::span<const std::byte>({reinterpret_cast<const std::byte*>(data->data()), data->size()}), std::move(data), cb, offset);
        }

        void async_write_some(int fd, std::span<const std::byte> data, std::shared_ptr<std::string>&& dataToKeep, Callback cb, int offset = 0){
            auto lk = std::lock_guard(_taskPostMutex);
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            io_uring_prep_write(task, fd, data.data(), data.size(), offset);
            auto i_callback = new intrusive_callback(std::move(cb), std::move(dataToKeep));
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if (res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_readv()");
            }
            active_callbacks.push_back(*i_callback);
        }

        void async_read(int fd, std::shared_ptr<std::string>&& data, std::size_t len, Callback cb, int offset = 0){
            static_assert(sizeof(decltype(*data->data())) == 1, "Buffer must have byte-sized elements.");
            {
                auto lk = std::lock_guard(m);
                std::cout << "async_read() started\n";
                std::cout << "async_read(): len = " << len << '\n';
            }
            if(len > 0){ //This means we still need to read something
                async_read_some(fd, {reinterpret_cast<std::byte*>(data->data() + data->size() - len), static_cast<unsigned long>(len)}, std::move(data), [fd, data, offset, this, len, cb](int res) mutable {
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        async_read(fd, std::move(data), len - res, cb, offset + res); //continue reading data
                    }
                }, offset);
            }
            else //read is complete
                cb(offset);
        }

        void async_write(int fd, std::shared_ptr<std::string>&& data, std::size_t len, Callback cb, int offset = 0){
            static_assert(sizeof(decltype(*data->data())) == 1, "Buffer must have byte-sized elements.");
            {
                auto lk = std::lock_guard(m);
                std::cout << "async_write() started\n";
                std::cout << "async_write(): len = " << len << '\n';
            }
            if(len > 0){ //This means we still need to write something
                async_write_some(fd, {reinterpret_cast<const std::byte*>(data->data() + data->size() - len), static_cast<unsigned long>(len)}, std::move(data), [fd, data, offset, this, len, cb](int res) mutable {
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        async_write(fd, std::move(data), len - res, cb, offset + res); //continue reading data
                    }
                }, offset);
            }
            else //read is complete
                cb(offset);
        }

        void async_read_until(int fd, std::shared_ptr<std::string>&& data, const std::string& delim, Callback cb, int offset = 0){
            async_read_until(fd, std::move(data), [delim](const std::string& d){
                auto res = std::search(d.begin(), d.end(), std::default_searcher(delim.begin(), delim.end()));
                if(res == d.end())
                    return ptrdiff_t(-1);
                else return res - d.begin() + 1;
            }, cb, offset);
        }

        void async_read_until(int fd, std::shared_ptr<std::string>&& data, Predicate pred, Callback cb, int offset = 0){
            static_assert(sizeof(decltype(*data->data())) == 1, "Buffer must have byte-sized elements.");
                if(std::ptrdiff_t match_len = pred(*data); match_len > 0){
                    //if already got match, call back.
                    cb(match_len);
                    return;
                }
                //if still no match,
                //check if we've got enough memory:
                if(data->size() == data->max_size())
                    cb(data->size()); //if no match is found, but we ran into max_size() then call back.

                //resize to maximum available length
                std::size_t len = std::min(data->max_size(), data->capacity()) - data->size();
                data->resize(data->size() + len);
                async_read_some(fd, {reinterpret_cast<std::byte*>(data->data() + data->size() - len), len}, std::move(data), [fd, data, offset, this, pred, cb, len](int res) mutable {
                    data->resize(data->size() - len + res);
                    if(res < 0)
                        cb(res);
                    else
                        async_read_until(fd, std::move(data), pred, cb, offset + res);
                }, offset);
        }

        void async_sock_accept(int fd, sockaddr* addr, socklen_t* len, int flags, Callback cb){
            io_uring_sqe* task = io_uring_get_sqe(&ring);
            io_uring_prep_accept(task, fd, addr, len, flags);
            auto* i_callback = new intrusive_callback(std::move(cb), std::shared_ptr<std::string>());
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if(res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_accept()");
            }
            active_callbacks.push_back(*i_callback);
        }

        void async_sock_connect(int fd, sockaddr* addr, socklen_t len, Callback cb){
            io_uring_sqe* task = io_uring_get_sqe(&ring);
            io_uring_prep_connect(task, fd, addr, len);
            auto* i_callback = new intrusive_callback(std::move(cb), std::shared_ptr<std::string>());
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if(res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_connect()");
            }
            active_callbacks.push_back(*i_callback);
        }

        std::function<void(void)> check_act(){
            io_uring_cqe *result = nullptr;
            io_uring_wait_cqe(&ring, &result);
            if (result) {
                auto lk = std::lock_guard(_taskPostMutex);
                auto* i_callback = reinterpret_cast<intrusive_callback *>(io_uring_cqe_get_data(result));
                Callback cb = i_callback->cb_;
                int callRes = result->res;
                std::function<void(void)> res = [cb, callRes](){cb(callRes);};
                active_callbacks.erase_and_dispose(active_callbacks.iterator_to(*i_callback),
                                                   std::default_delete<intrusive_callback>());
                    io_uring_cqe_seen(&ring, result);
                return res;
            }
            return check_act();
        }

        ~uring_wrapper(){
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


#endif //URING_TCP_SERVER_ASYNC_URING_H
