cmake_minimum_required(VERSION 3.25)
project(IntelliHostServer LANGUAGES CXX)

set(INTELLIHOST_SOURCES
        IntelliHost.cpp
        ../Client/SocketSTL.cpp
        ../Client/tinyxml2.cpp
        ../Client/SHA256.cpp
)

add_executable(IntelliHostServer ${INTELLIHOST_SOURCES})

target_compile_features(IntelliHostServer PRIVATE cxx_std_26)

find_package(Threads REQUIRED)

if(WIN32)
    target_link_libraries(IntelliHostServer PRIVATE Threads::Threads ws2_32 secur32)
else()
    target_link_libraries(IntelliHostServer PRIVATE Threads::Threads)
endif()
