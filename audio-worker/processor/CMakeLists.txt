cmake_minimum_required(VERSION 3.16)
project(wav_processor LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(wav_processor 
     wav_processor.cpp
     kissfft/kiss_fft.c
     kissfft/kiss_fftr.c
)

target_include_directories(wav_processor PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/kissfft
)


install(TARGETS wav_processor DESTINATION bin)

