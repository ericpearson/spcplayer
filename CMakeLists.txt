cmake_minimum_required(VERSION 3.16)
project(spcplayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL2
find_package(SDL2 REQUIRED)

add_executable(spcplayer
    src/main.cpp
    src/player/spc_player.cpp
    src/cpu/spc700.cpp
    src/cpu/spc700_opcodes.cpp
    src/dsp/dsp.cpp
    src/loader/spc_file.cpp
    src/loader/id666.cpp
    src/audio/audio_output.cpp
    src/audio/resampler.cpp
)

target_include_directories(spcplayer PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(spcplayer PRIVATE ${SDL2_LIBRARIES})

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(spcplayer PRIVATE -Wall -Wextra -Wpedantic)
endif()
