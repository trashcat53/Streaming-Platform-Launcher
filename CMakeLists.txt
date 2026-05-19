cmake_minimum_required(VERSION 3.22)
project(StreamingService CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.1)

add_executable(StreamingService main.cpp)

target_include_directories(StreamingService PRIVATE
        ${GTK4_INCLUDE_DIRS}
        ${WEBKIT_INCLUDE_DIRS}
)

target_link_libraries(StreamingService PRIVATE
        PkgConfig::GTK4
        PkgConfig::WEBKIT
)