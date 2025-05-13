cmake_minimum_required(VERSION 3.19)
project(TwoBot)

set(CMAKE_CXX_STANDARD 20)


add_library(TwoBot STATIC
        src/twobot.hh
        src/twobot.cc
        src/apiset.cc
        src/ThreadSafeMap.hh
)

target_include_directories(TwoBot PUBLIC src/)

find_package(OpenSSL REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(BRYNET_INCLUDE_DIRS "brynet/Version.hpp")

target_include_directories(TwoBot PRIVATE ${BRYNET_INCLUDE_DIRS})

target_link_libraries(TwoBot PUBLIC nlohmann_json::nlohmann_json httplib::httplib OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(TwoBot PUBLIC _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING CPPHTTPLIB_OPENSSL_SUPPORT)

add_executable(TwoBot-demo demo/main.cc)
target_link_libraries(TwoBot-demo TwoBot)

if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif()
