cmake_minimum_required(VERSION 3.16)
project(PS4MarketNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{OO_PS4_TOOLCHAIN})
  message(FATAL_ERROR "Set OO_PS4_TOOLCHAIN to your OpenOrbis toolchain directory.")
endif()

find_package(SDL2 REQUIRED)
add_executable(eboot src/main.cpp src/platform_ps4.cpp src/http_download.cpp src/sha256.cpp src/catalog.cpp)
target_include_directories(eboot PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(eboot PRIVATE ${SDL2_LIBRARIES})
