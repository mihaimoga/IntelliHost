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

target_compile_options(IntelliHostServer PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
)

configure_file(
        IntelliHost.xml
        IntelliHost.xml
        COPYONLY
)

# Optional MySQL demo; built only when the MySQL C client library is found
find_path(MYSQL_INCLUDE_DIR mysql.h PATH_SUFFIXES mysql mariadb)
find_library(MYSQL_LIBRARY NAMES mysqlclient mariadbclient libmysql)
if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    add_executable(IntelliDemo IntelliDemo.cpp ../Client/tinyxml2.cpp)
    target_compile_features(IntelliDemo PRIVATE cxx_std_26)
    target_include_directories(IntelliDemo PRIVATE ${MYSQL_INCLUDE_DIR})
    target_link_libraries(IntelliDemo PRIVATE ${MYSQL_LIBRARY})
    if(WIN32)
        target_link_libraries(IntelliDemo PRIVATE ws2_32)
    endif()
else()
    message(STATUS "MySQL client library not found - IntelliDemo will not be built")
endif()
