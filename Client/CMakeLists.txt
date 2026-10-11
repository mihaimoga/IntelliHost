cmake_minimum_required(VERSION 3.25)

project(IntelliHost LANGUAGES CXX)

include(FetchContent)

find_package(tinyxml2 CONFIG QUIET)
if(NOT tinyxml2_FOUND)
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tinyxml2 tests" FORCE)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG 10.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(tinyxml2)
endif()

add_executable(IntelliHost IntelliHost.cpp
        tinyxml2.h)
target_compile_features(IntelliHost PRIVATE cxx_std_26)
target_compile_options(IntelliHost PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)
target_link_libraries(IntelliHost PRIVATE tinyxml2::tinyxml2)

configure_file(IntelliHost.xml IntelliHost.xml COPYONLY)
