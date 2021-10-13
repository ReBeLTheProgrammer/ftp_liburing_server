cmake_minimum_required(VERSION 3.16)
project(uring_tcp_server)

add_executable(uring_tcp_server
        src/main.cpp
        src/async_uring.cpp
        src/FTPServerController.cpp
        src/FTPConnection.cpp
        src/FTPConnectionState.cpp
        src/FTPFileSystem.cpp
        src/FTPCommon.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

set(BOOST_ROOT ${CMAKE_CURRENT_LIST_DIR}/../prefix/boost)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)

find_package(uring REQUIRED)
find_package(Boost 1.75 REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC uring Boost::boost)