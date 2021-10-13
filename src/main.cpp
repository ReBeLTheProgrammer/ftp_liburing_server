#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <boost/asio/thread_pool.hpp>
#include <FTPServerController.h>

std::mutex m;
//
//void check_async_io_some(std::shared_ptr<mp::uring_wrapper> ring){
//    {
//        auto lk = std::lock_guard(m);
//        std::cout << "check_async_io_some() starts.\n";
//    }
//    int fd1, fd2;
//    {
//        auto lk = std::lock_guard(m);
//        fd1 = open("tmp1.txt", O_CREAT | O_RDWR, 0664);
//        fd2 = open("tmp2.txt", O_CREAT | O_RDWR, 0664);
//    }
//    if(fd1 < 0 || fd2 < 0)
//        throw std::system_error(errno, std::system_category(), "open()");
//    auto _msg1 = std::make_shared<std::string>("Hello, World1!\n"), _msg2 = std::make_shared<std::string>("Hello, World2!\n");
//    ring->async_write_some(fd1, std::move(_msg1), [](int result){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io_some()]: Write operation on fd1 finished with result " << result << '\n';
//    });
//    ring->async_write_some(fd2, std::move(_msg2), [](int result){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io_some()]: Write operation on fd2 finished with result " << result << '\n';
//    });
//    auto msg1 = std::make_shared<std::string>(), msg2 = std::make_shared<std::string>();
//    msg1->resize(20, 0);
//    msg2->resize(20, 0);
//    ring->async_read_some(fd1, std::move(msg1), [msg1, fd1](int result){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io_some()]: Read operation on fd1 finished with result " << result << ".\n The message is: " << *msg1 << '\n';
//        close(fd1);
//        remove("tmp1.txt");
//    });
//    ring->async_read_some(fd2, std::move(msg2), [msg2, fd2](int result){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io_some()]: Read operation on fd2 finished with result " << result << ".\n The message is: " << *msg2 << '\n';
//        close(fd2);
//        remove("tmp2.txt");
//    });
//}
//
//void check_async_io(std::shared_ptr<mp::uring_wrapper> ring){
//    {
//        auto lk = std::lock_guard(m);
//        std::cout << "check_async_io() starts.\n";
//    }
//    int fd1, fd2;
//    {
//        auto lk = std::lock_guard(m);
//        fd2 = open("tmp.txt", O_CREAT | O_WRONLY | O_APPEND, 0664);
//        fd1 = open("tmp.txt", O_CREAT | O_RDONLY, 0664);
//    }
//    auto str = std::make_shared<std::string>();
//    str->resize(20, 0);
//    auto msg1 = std::make_shared<std::string>("Hello!\n"), msg2 = std::make_shared<std::string>("Name's Petya\n");
//    ring->async_write(fd2, std::move(msg1), msg1->length(), [](int res){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io()]: Write operation complete, result:" << res << '\n';
//    });
//    ring->async_read(fd1, std::move(str), str->size(), [str, fd1, fd2](int res){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io()]: async_read complete, result: " << res << ", the message is:\n" << *str << '\n';
//        close(fd1);
//        close(fd2);
//        remove("tmp.txt");
//    });
//    ring->async_write(fd2, std::move(msg2), msg2->length(), [](int res){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_io()]: Write operation complete, result:" << res << '\n';
//    });
//
//}
//
//void check_async_read_until(std::shared_ptr<mp::uring_wrapper> ring){
//    std::cout << "check_async_read_until() starts.\n";
//    int fd1 = open("tmp3.txt", O_CREAT | O_RDWR, 0664);
//    auto msg = std::make_shared<std::string>();
//    msg->reserve(200);
//    std::vector<std::shared_ptr<std::string>> msgs = {
//            std::make_shared<std::string>("Hi!\n"),
//            std::make_shared<std::string>("WYD?\n"),
//            std::make_shared<std::string>("Just chillin'\n"),
//            std::make_shared<std::string>("Nice\n"),
//            std::make_shared<std::string>("Gotcha!")};
//    int offset = 0;
//    for(auto& s: msgs) {
//        ring->async_write(fd1, std::move(s), s->length(), [s](std::size_t res) mutable { auto lk = std::lock_guard(m); std::cout << "[check_async_read_until()]: Wrote " << *s << '\n'; }, offset);
//        offset += s->length();
//    }
//    ring->async_read_until(fd1, std::move(msg), "o", [msg, fd1](std::size_t res){
//        auto lk = std::lock_guard(m);
//        std::cout << "[check_async_read_until()]: read_until() finished, message: " << *msg << '\n';
//        close(fd1);
//        remove("tmp3.txt");
//    });
//}
//
//void test_uring(){
//    boost::asio::thread_pool pool(2);
//    auto ring = std::make_shared<mp::uring_wrapper>(16);
//    pool.executor().post([&pool, ring](){
//                             while(true) {
//                                 pool.executor().post(ring->check_act(), std::allocator<void>());
//                                 {
//                                     auto lk = std::lock_guard(m);
//                                     std::cout << "Posted a new callback to the pool\n";
//                                 }
//                             }},
//                         std::allocator<void>());
//    pool.executor().post([ring](){ check_async_io_some(ring); }, std::allocator<void>());
//    pool.executor().post([ring](){ check_async_io(ring); }, std::allocator<void>());
//    pool.executor().post([ring](){ check_async_read_until(ring); }, std::allocator<void>());
//    pool.join();
//}

int main() {
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = 25554;
    address.sin_addr.s_addr = inet_addr("192.168.178.36");
    auto controller = mp::FTPServerController(address);
    controller.start();
    char c;
    std::cin >> c;
    controller.stop();
    return 0;
}
