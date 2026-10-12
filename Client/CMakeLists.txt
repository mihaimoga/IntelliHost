cmake_minimum_required(VERSION 3.25)

project(IntelliHost LANGUAGES CXX)

set(INTELLIHOST_SOURCES
    IntelliHost.cpp
    SocketSTL.cpp
    tinyxml2.cpp
)

add_executable(IntelliHost ${INTELLIHOST_SOURCES})

target_compile_features(IntelliHost PRIVATE cxx_std_26)

target_include_directories(IntelliHost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(IntelliHost PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)

configure_file(
    IntelliHost.xml
    IntelliHost.xml
    COPYONLY
)
