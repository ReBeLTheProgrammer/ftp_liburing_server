#ifndef URING_TCP_SERVER_ASYNC_URING_H
#define URING_TCP_SERVER_ASYNC_URING_H

#include <liburing.h>
#include <string>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <system_error>
#include <utility>
#include <queue>
#include <span>
#include <functional>
#include <boost/intrusive/list.hpp>
#include <cassert>
#include <iostream>
#include <memory>

namespace mp{

    using Callback = std::function<void(int)>;

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
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            iovec io_vec = {.iov_base = data.data(), .iov_len = data.length()};
            int *id = new int(fd);
            io_uring_prep_read(task, fd, data.data() + offset, data.size() - offset, offset);
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
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            int *id = new int(fd);
            io_uring_prep_write(task, fd, data.data() + offset, data.length() - offset, offset);
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
            if(offset < len){ //This means we still need to read something
                async_read_some(fd, data, [fd, &data, offset, this, len, cb](int res){
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        if(res + offset < len)
                            async_read(fd, data, len, cb, offset + res); //continue reading data
                        else
                            cb(res + offset);
                    }
                }, offset);
                should_continue_waiting = true;
            }
            else //read is complete
                cb(data.length());
        }

        void async_write(int fd, const std::string& data, int len, Callback cb, int offset = 0){
            if(offset < len){ //This means we still need to read something
                async_write_some(fd, data, [fd, &data, offset, this, len, cb](int res){
                    if(res < 0){//something bad
                        cb(res);
                    } else { //successfully read some data, probably still have something to read
                        if(res + offset < len)
                            async_write(fd, data, len, cb, offset + res); //continue reading data
                        else
                            cb(res + offset);
                    }
                }, offset);
                should_continue_waiting = true;
            }
            else //read is complete
                cb(data.length());
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
