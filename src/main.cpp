#include <iostream>
#include <Server.h>

int main() {
    signal(SIGPIPE, SIG_IGN);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = 25554;
    address.sin_addr.s_addr = inet_addr("192.168.178.36");
    auto controller = ftp::Server(address, std::filesystem::current_path(), 8);
    try {
        controller.start();
    } catch(std::exception& e){
        std::cerr << "Caught an exception on controller.start():" << e.what() << '\n';
        controller.stop();
        return 1;
    }
    char c;
    std::cin >> c;
    controller.stop();
    return 0;
}
