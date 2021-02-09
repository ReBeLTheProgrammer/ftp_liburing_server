//
// Created by therbl on 1/29/21.
//

#ifndef URING_TCP_SERVER_ASYNC_URING_H
#define URING_TCP_SERVER_ASYNC_URING_H

#include <liburing.h>
#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <exception>
#include <iostream>
#include <cstring>

namespace mp {

    class uring_wrapper{
    public:
        enum uring_type: int {
            interrupted = 0,
            io_poll = IORING_SETUP_IOPOLL,
            sq_poll = IORING_SETUP_SQPOLL
        };

        uring_wrapper(int entries, uring_type type){
            assert(
                    entries == 1UL ||
                    entries == 1UL << 1 ||
                    entries == 1UL << 2 ||
                    entries == 1UL << 3 ||
                    entries == 1UL << 4 ||
                    entries == 1UL << 5 ||
                    entries == 1UL << 6 ||
                    entries == 1UL << 7 ||
                    entries == 1UL << 8 ||
                    entries == 1UL << 9 ||
                    entries == 1UL << 10 ||
                    entries == 1UL << 11 ||
                    entries == 1UL << 12
                    );
            int res = io_uring_queue_init(entries, &ring, type);
            if(res < 0) {
                perror("io_uring_queue_init()");
                throw std::runtime_error("Error initializing io_uring");
            }
        }

        void async_read_some(int fd, char* buf, std::size_t size){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            if(task == NULL)
                throw std::runtime_error("Error: Submission queue is full.");
            iovec vec {.iov_base = buf, .iov_len = size};
            io_uring_prep_readv(task, fd, &vec, 1, 0);
            int res = io_uring_submit(&ring);
            if(res < 0)
                perror("read_some()/submit()");
        }

        void async_write_some(int fd, char* buf, std::size_t size){
            io_uring_sqe *task = io_uring_get_sqe(&ring);
            if(task == NULL)
                throw std::runtime_error("Error: Submission queue is full.");
            iovec vec {.iov_base = buf, .iov_len = size};
            io_uring_prep_writev(task, fd, &vec, 1, 0);
            int res = io_uring_submit(&ring);
            if(res < 0)
                perror("read_some()/submit()");
        }

        void wait_completion(){
            io_uring_cqe* completionEntry;
            if(io_uring_wait_cqe(&ring, &completionEntry) < 0)
                throw std::runtime_error("wait_completion() error.");
            std::cout << "Some operation completed. The result is: " << completionEntry->res << "\n";
            io_uring_cqe_seen(&ring, completionEntry);
        }

        ~uring_wrapper(){
            io_uring_queue_exit(&ring);
        }

    private:
        io_uring ring;
    };

    class connection{

        connection(int sockfd): _socket{sockfd} {
            if(ring.ring_fd == -1){
                io_uring_queue_init(1024, &ring, 0);
            }
        }
        void handle_read();

        void handle_write();
    private:
        int _socket;
        std::string _data;
        static io_uring ring;
    };

    io_uring connection::ring = {io_uring_sq{}, io_uring_cq{}, 0, -1};

}
#endif //URING_TCP_SERVER_ASYNC_URING_H
