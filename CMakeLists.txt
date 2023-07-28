cmake_minimum_required(VERSION 3.24)
project(factor_ecm)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/safeheron")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/safeheron")

cmake_policy(SET CMP0076 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0079 NEW)

find_package(PkgConfig REQUIRED)
pkg_search_module(PROTOBUF REQUIRED protobuf)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)
find_package(SafeheronCryptoSuites REQUIRED)

include_directories(
        ${PROTOBUF_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS}
        ${SafeheronCryptoSuites_INCLUDE_DIRS}
        )

link_directories(/usr/local/lib)
link_libraries(
        ${PROTOBUF_LIBRARIES}
        OpenSSL::Crypto
        ${Boost_LIBRARIES}
        SafeheronCryptoSuites
        -ldl)

add_executable(factor_ecm main.cpp)

install(TARGETS factor_ecm
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)