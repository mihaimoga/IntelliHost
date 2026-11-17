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
