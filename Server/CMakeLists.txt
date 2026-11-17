cmake_minimum_required(VERSION 3.25)
project(IntelliHostServer LANGUAGES CXX)

add_executable(IntelliHostServer IntelliHost.cpp)

target_compile_features(IntelliHostServer PRIVATE cxx_std_26)
