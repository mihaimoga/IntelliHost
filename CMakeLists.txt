cmake_minimum_required(VERSION 3.25)

project(IntelliHost LANGUAGES CXX)

enable_testing()

add_subdirectory(Server)
add_subdirectory(Client)
