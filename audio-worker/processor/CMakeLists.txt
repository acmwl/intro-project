cmake_minimum_required(VERSION 3.16)
project(wav_processor)

set(CMAKE_CXX_STANDARD 17)

add_executable(wav_processor wav_processor.cpp)

install(TARGETS wav_processor DESTINATION bin)
