#include "async_uring.h"
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

int main() {
    mp::uring_wrapper ring(8, mp::uring_wrapper::uring_type::interrupted);
    char str[40], msg[] = "Hello, World!\n";
    memset(str, 0, 40);
    int fd = open("tmp.txt", O_RDWR);
    if (fd == -1){
        perror("open()");
        return 1;
    }
    std::cout << "File opened on fd: " << fd << '\n';
    ring.async_write_some(fd, msg, strlen(msg));
    std::cout << "waiting for async_write_some() to finish...\n";
    ring.wait_completion();

    ring.async_read_some(fd, str, 40);
    std::cout << "waiting for async_read_some() to finish...\n";
    ring.wait_completion();
    std::cout << str;
    close(fd);
    return 0;
}
