cmake_minimum_required(VERSION 3.19)
project(TwoBot)

set(CMAKE_CXX_STANDARD 20)


add_library(TwoBot STATIC
        src/twobot.hh
        src/twobot.cc
        src/apiset.cc
        src/tsumap.hh
)


find_package(OpenSSL REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(BRYNET_INCLUDE_DIRS "brynet/Version.hpp")

target_include_directories(TwoBot PRIVATE ${BRYNET_INCLUDE_DIRS})

target_link_libraries(TwoBot PUBLIC nlohmann_json::nlohmann_json httplib::httplib OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(TwoBot PUBLIC _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING CPPHTTPLIB_OPENSSL_SUPPORT BRYNET_USE_OPENSSL)

add_executable(TwoBot-demo demo/main.cc)
target_link_libraries(TwoBot-demo TwoBot)

target_include_directories(TwoBot PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>   # for headers when building
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # for client in install mode
)

install(TARGETS TwoBot
            EXPORT TwoBot_targets
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY src/ DESTINATION include
            FILES_MATCHING PATTERN "*.hh")

install(EXPORT TwoBot_targets
        FILE TwoBot-targets.cmake
        NAMESPACE TwoBot::
        DESTINATION share/TwoBot)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/TwoBot-config.cmake"
"include(CMakeFindDependencyMacro)\n"
"find_dependency(OpenSSL REQUIRED)\n"
"find_dependency(httplib CONFIG REQUIRED)\n"
"find_dependency(nlohmann_json CONFIG REQUIRED)\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/TwoBot-targets.cmake\")\n"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TwoBot-config.cmake"
        DESTINATION share/TwoBot)

if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif()
