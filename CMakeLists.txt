cmake_minimum_required(VERSION 3.10)

project(StreamingService)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(VLC REQUIRED libvlc)

add_executable(main main.cpp)

target_include_directories(main PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${VLC_INCLUDE_DIRS}
)

target_link_directories(main PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${SDL2_TTF_LIBRARY_DIRS}
        ${VLC_LIBRARY_DIRS}
)

target_link_libraries(main PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${VLC_LIBRARIES}
)