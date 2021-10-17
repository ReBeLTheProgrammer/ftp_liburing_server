#include <iostream>
#include <Server.h>
#include <boost/program_options.hpp>

int main(int argc, char* argv[]) {

    std::uint16_t port = -1;
    unsigned threadCount = -1;

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
            ("help", "print this help message")
            ("threads", boost::program_options::value<unsigned>(&threadCount)->default_value(std::thread::hardware_concurrency()), "set the maximum cores to be used")
            ("port", boost::program_options::value<std::uint16_t>(&port), "set the port for the control connections");

    boost::program_options::variables_map options;

    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), options);
    boost::program_options::notify(options);

    if(options.count("help")){
        std::cout << desc << '\n';
        return 1;
    }

    std::cout << "port: " << port << "\nthreads: " << threadCount << '\n';

    signal(SIGPIPE, SIG_IGN);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr("192.168.178.36");
    static auto controller = ftp::Server(address, std::filesystem::current_path(), threadCount);

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
