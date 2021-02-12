#include "async_uring.h"
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

void check_async_io_some(){
    int fd1 = -1, fd2 = -1;
    fd1 = open("tmp1.txt", O_CREAT | O_RDWR, 0664);
    fd2 = open("tmp2.txt", O_CREAT | O_RDWR, 0664);
    std::cout << fd1 << ' ' << fd2 << '\n';
    if(fd1 < 0 || fd2 < 0)
        throw std::system_error(errno, std::system_category(), "open()");
    mp::uring_wrapper ring(2, mp::uring_wrapper::uring_mode::interrupted);
    ring.async_write_some(fd1, std::string("Hello, World1!\n"), [](int result){std::cout << "Write operation on fd1 finished with result " << result << '\n';});
    ring.async_write_some(fd2, std::string("Hello, World2!\n"), [](int result){std::cout << "Write operation on fd2 finished with result " << result << '\n';});
    ring.check_act();
    ring.check_act();
    std::string msg1, msg2;
    msg1.resize(20, 0);
    msg2.resize(20, 0);
    ring.async_read_some(fd1, msg1, [&msg1](int result){std::cout << "Read operation on fd1 finished with result " << result << ".\n The message is: " << msg1 << '\n';});
    ring.async_read_some(fd2, msg2, [&msg2](int result){std::cout << "Read operation on fd2 finished with result " << result << ".\n The message is: " << msg2 << '\n';});
    ring.check_act();
    ring.check_act();
    close(fd1);
    close(fd2);
}

int main() {
    int fd = 0;
    mp::uring_wrapper ring(4, mp::uring_wrapper::uring_mode::interrupted);
    std::string str;
    str.resize(20, 0);
    ring.async_read(fd, str, str.size(), [&str](int res){
        std::cout << "async_read complete, result: " << res << ", the message is:\n" << str << '\n';
    });
    while(1)
        ring.check_act();
    return 0;
}
