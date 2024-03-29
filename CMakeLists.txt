cmake_minimum_required(VERSION 3.16)

project(AsyncFTPServer)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/AsyncUring.cpp
        src/Server.cpp
        src/ControlConnection.cpp
        src/ConnectionState.cpp
        src/FileSystemProxy.cpp
        src/Common.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC include/)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)

find_package(uring REQUIRED)
find_package(Boost 1.75 REQUIRED COMPONENTS program_options)

target_link_libraries(${PROJECT_NAME} PUBLIC uring Boost::boost Boost::program_options)