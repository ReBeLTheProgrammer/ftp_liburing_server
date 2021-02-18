#ifndef URING_TCP_SERVER_ASYNC_URING_H
#define URING_TCP_SERVER_ASYNC_URING_H

#include <liburing.h>
#include <string>
#include <cstdint>
#include <system_error>
#include <utility>
#include <span>
#include <functional>
#include <boost/intrusive/list.hpp>
#include <memory>

namespace mp{

    using Callback = std::function<void(std::size_t)>;
    using Predicate = std::function<std::ptrdiff_t(std::string)>;

    class uring_wrapper{
    public:
        enum uring_mode{
            interrupted = 0,
            io_polled = IORING_SETUP_IOPOLL,
            sq_polled = IORING_SETUP_SQPOLL
        };
        uring_wrapper(int size, uring_mode mode){
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
            int res = io_uring_queue_init(size, &ring, mode);
            should_continue_waiting = false;
            if(res < 0) {
                throw std::system_error(errno, std::system_category(), "uring_wrapper()");
            }
        }

        void async_read_some(int fd, std::string& data, Callback cb, int offset = 0){
            async_read_some(fd, std::span<std::byte>({reinterpret_cast<std::byte*>(data.data()), data.size()}), cb, offset);
        }

        void async_read_some(int fd, std::span<std::byte> data, Callback cb, int offset = 0){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            iovec io_vec = {.iov_base = data.data(), .iov_len = data.size()};
            int *id = new int(fd);
            io_uring_prep_read(task, fd, data.data(), data.size(), offset);
            intrusive_callback* i_callback = new intrusive_callback(cb);
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if(res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_readv()");
            }
            active_callbacks.push_back(*i_callback);
        }

        void async_write_some(int fd, const std::string& data, Callback cb, int offset = 0){
            async_write_some(fd, std::span<const std::byte>({reinterpret_cast<const std::byte*>(data.data()), data.size()}), cb, offset);
        }

        void async_write_some(int fd, std::span<const std::byte> data, Callback cb, int offset = 0){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            int *id = new int(fd);
            io_uring_prep_write(task, fd, data.data(), data.size(), offset);
            intrusive_callback* i_callback = new intrusive_callback(cb);
            io_uring_sqe_set_data(task, i_callback);
            int res = io_uring_submit(&ring);
            if(res < 0) {
                delete i_callback;
                throw std::system_error(errno, std::system_category(), "post_readv()");
            }
            active_callbacks.push_back(*i_callback);
        }

        void async_read(int fd, std::string& data, int len, Callback cb, int offset = 0){
            if(len > 0){ //This means we still need to read something
                async_read_some(fd, {reinterpret_cast<std::byte*>(data.data() + data.size() - len), static_cast<unsigned long>(len)}, [fd, &data, offset, this, len, cb](int res){
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        async_read(fd, data, len - res, cb, offset + res); //continue reading data
                    }
                }, offset);
                should_continue_waiting = true;
            }
            else //read is complete
                cb(data.length());
        }

        void async_write(int fd, const std::string& data, int len, Callback cb, int offset = 0){
            if(len > 0){ //This means we still need to write something
                async_write_some(fd, {reinterpret_cast<const std::byte*>(data.data() + data.size() - len), static_cast<unsigned long>(len)}, [fd, &data, offset, this, len, cb](int res){
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        async_write(fd, data, len - res, cb, offset + res); //continue reading data
                    }
                }, offset);
                should_continue_waiting = true;
            }
            else //read is complete
                cb(data.length());
        }

        void async_read_until(int fd, std::string& data, Predicate pred, Callback cb, int offset = 0){
                if(std::ptrdiff_t match_len = pred(data); match_len > 0){
                    //if already got match, call back.
                    cb(match_len);
                    return;
                }
                //if still no match,
                //check if we've got enough memory:
                if(data.size() == data.max_size())
                    cb(data.size()); //if no match is found, but we ran into max_size() then call back.

                //resize to maximum available length
                std::size_t len = std::min(data.max_size(), data.capacity()) - data.size();
                data.resize(data.size() + len);
                async_read_some(fd, {reinterpret_cast<std::byte*>(data.data() + data.size() - len), len}, [fd, &data, offset, this, pred, cb, len](int res){
                    data.resize(data.size() - len + res);
                    if(res < 0)
                        cb(res);
                    else
                        async_read_until(fd, data, pred, cb, offset + res);
                }, offset);
            should_continue_waiting = true;
        }

        void check_act(){
            io_uring_cqe *result;
            do {
                should_continue_waiting = false;
                result = nullptr;
                io_uring_wait_cqe(&ring, &result);
                intrusive_callback *i_callback = reinterpret_cast<intrusive_callback *>(io_uring_cqe_get_data(result));
                if (result) {
                    i_callback->cb_(result->res);
                    active_callbacks.erase_and_dispose(active_callbacks.iterator_to(*i_callback),
                                                       std::default_delete<intrusive_callback>());
                }
                io_uring_cqe_seen(&ring, result);
            } while(should_continue_waiting);
        }

        ~uring_wrapper(){
            active_callbacks.clear_and_dispose(std::default_delete<intrusive_callback>());
            io_uring_queue_exit(&ring);
        }

    private:
        io_uring ring;
        bool should_continue_waiting;

    class intrusive_callback: public boost::intrusive::list_base_hook<> {
        public:
            intrusive_callback(Callback cb): cb_(cb) {}
            Callback cb_;
        };

        boost::intrusive::list<intrusive_callback> active_callbacks;
    };

}


#endif //URING_TCP_SERVER_ASYNC_URING_H
