cmake_minimum_required(VERSION 3.20)
project(StreamingService CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED imported_target gtk4)
pkg_check_modules(WEBKIT REQUIRED imported_target webkitgtk-6.0)

add_executable(stream_launcher main.cpp)

target_link_libraries(stream_launcher
        PRIVATE
        PkgConfig::GTK4
        PkgConfig::WEBKIT
)