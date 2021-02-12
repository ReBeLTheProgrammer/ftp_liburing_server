#include "async_uring.h"
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

int main() {
    int fd1 = -1, fd2 = -1;
    fd1 = open("tmp1.txt", O_CREAT | O_RDWR, 0664);
    fd2 = open("tmp2.txt", O_CREAT | O_RDWR, 0664);
    std::cout << fd1 << ' ' << fd2 << '\n';
    if(fd1 < 0 || fd2 < 0)
        throw std::system_error(errno, std::system_category(), "open()");
    mp::uring_wrapper ring(2, mp::uring_wrapper::uring_mode::interrupted);
    ring.post_writev(fd1, std::string("Hello, World1!\n"), [](int result){std::cout << "Write operation on fd1 finished with result " << result << '\n';});
    ring.post_writev(fd2, std::string("Hello, World2!\n"), [](int result){std::cout << "Write operation on fd2 finished with result " << result << '\n';});
    ring.check_act();
    ring.check_act();
    std::string msg1, msg2;
    msg1.resize(20, 0);
    msg2.resize(20, 0);
    ring.post_readv(fd1, msg1, [&msg1](int result){std::cout << "Read operation on fd1 finished with result " << result << ".\n The message is: " << msg1 << '\n';});
    ring.post_readv(fd2, msg2, [&msg2](int result){std::cout << "Read operation on fd2 finished with result " << result << ".\n The message is: " << msg2 << '\n';});
    ring.check_act();
    ring.check_act();
    close(fd1);
    close(fd2);
    return 0;
}
