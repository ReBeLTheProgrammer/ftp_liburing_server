cmake_minimum_required(VERSION 3.16)
project(uring_tcp_server)

add_executable(uring_tcp_server main.cpp async_uring.h async_uring.cpp)# FTPServerController.cpp FTPServerController.h FTPConnection.cpp FTPConnection.h FTPConnectionState.cpp FTPConnectionState.h FTPCommon.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

set(BOOST_ROOT ${CMAKE_CURRENT_LIST_DIR}/../prefix/boost)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)

find_package(uring REQUIRED)
find_package(Boost 1.75 REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC uring Boost::boost)