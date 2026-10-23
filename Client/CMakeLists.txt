cmake_minimum_required(VERSION 3.25)

project(IntelliHost LANGUAGES CXX)

set(INTELLIHOST_SOURCES
    IntelliHost.cpp
    SocketSTL.cpp
    tinyxml2.cpp
    SHA256.cpp
)

add_executable(IntelliHost ${INTELLIHOST_SOURCES})

target_compile_features(IntelliHost PRIVATE cxx_std_26)

target_include_directories(IntelliHost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(IntelliHost PRIVATE ws2_32)
endif()

target_compile_options(IntelliHost PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)

configure_file(
    IntelliHost.xml
    IntelliHost.xml
    COPYONLY
)

# Unit tests for CSocketSTL (built together with the application; run with ctest)
enable_testing()

add_executable(IntelliTest
     IntelliTest.cpp
     SocketSTL.cpp
)

target_compile_features(IntelliTest PRIVATE cxx_std_26)

target_include_directories(IntelliTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(IntelliTest PRIVATE ws2_32)
endif()

add_test(NAME IntelliTest COMMAND IntelliTest)

# Build and run the unit tests first; a failing test aborts the IntelliHost build
add_custom_target(RunIntelliTest
    COMMAND IntelliTest
    DEPENDS IntelliTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running IntelliTest unit tests"
    VERBATIM
)
add_dependencies(IntelliHost RunIntelliTest)
