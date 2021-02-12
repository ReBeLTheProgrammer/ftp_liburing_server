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
#include <map>
#include <cassert>
#include <iostream>

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
            if(res < 0) {
                throw std::system_error(errno, std::system_category(), "uring_wrapper()");
            }
        }

        /**
         * post_readv
         * @brief creates a readv task for the fd and passed data span. Also binds c callback for the fd for check_act().
         * @tparam Callback - type of callback function to be used
         * @param fd - opened file descriptor
         * @param data - span, where the data will be put
         * @param cb - callback function
         */
        void async_read_some(int fd, std::string& data, Callback cb, int offset = 0){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            iovec io_vec = {.iov_base = data.data(), .iov_len = data.length()};
            int *id = new int(fd);
            io_uring_prep_read(task, fd, data.data() + offset, data.size() - offset, 0);
            io_uring_sqe_set_data(task, id);
            int res = io_uring_submit(&ring);
            if(res < 0)
                throw std::system_error(errno, std::system_category(), "post_readv()");
            if(!active_callbacks.contains(*id)) {
                active_callbacks.emplace(std::pair<int, std::queue<Callback>>(*id, std::queue<Callback>()));
            }
            active_callbacks[*id].push(std::move(cb));
        }

        /**
         * post_writev
         * @brief creates a writev task for the fd and passed data span. Also binds c callback for the fd for check_act().
         * @tparam Callback - type of callback function to be used
         * @param fd - opened file descriptor
         * @param data - span, from where the data will be used
         * @param cb - callback function
         */
        void async_write_some(int fd, std::string data, Callback cb, int offset = 0){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            int *id = new int(fd);
            io_uring_prep_write(task, fd, data.data() + offset, data.length() - offset, 0);
            io_uring_sqe_set_data(task, id);
            int res = io_uring_submit(&ring);
            if(res < 0)
                throw std::system_error(errno, std::system_category(), "post_readv()");
            if(!active_callbacks.contains(*id))
                active_callbacks.emplace(std::pair<int, std::queue<Callback>>(*id, std::queue<Callback>()));
            active_callbacks[*id].push(std::move(cb));
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
            }
            else //read is complete
                cb(data.length());
        }

        /**
         * check_act
         * @brief Checks if there are completed operations on the completion queue and calls relevant callbacks from active_callbacks map.
         */
        void check_act(){
            io_uring_cqe *result = nullptr;
            io_uring_wait_cqe(&ring, &result);
            int *id = reinterpret_cast<int *>(io_uring_cqe_get_data(result));
            if(result){
                if(active_callbacks.contains(*id)) {
                    active_callbacks[*id].front()(result->res);
                    active_callbacks[*id].pop();
                    if(active_callbacks[*id].empty())
                        active_callbacks.erase(*id);
                }
                delete ((int*)io_uring_cqe_get_data(result));
            }
            io_uring_cqe_seen(&ring, result);
        }

        ~uring_wrapper(){
            io_uring_queue_exit(&ring);
        }

    private:
        io_uring ring;
        ///This stands as a map of callbacks to be called when an i/o operation is complete for some fd.
        std::map<int, std::queue<std::function<void(int)>>> active_callbacks;
    };

}


#endif //URING_TCP_SERVER_ASYNC_URING_H
