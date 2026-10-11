cmake_minimum_required(VERSION 3.25)

project(IntelliHost LANGUAGES CXX)

add_executable(IntelliHost IntelliHost.cpp
        IntelliHost.cpp)
target_compile_features(IntelliHost PRIVATE cxx_std_26)

target_compile_options(IntelliHost PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)
