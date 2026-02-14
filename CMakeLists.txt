cmake_minimum_required(VERSION 3.19)
project(twobot)

set(CMAKE_CXX_STANDARD 20)


add_library(twobot STATIC
        src/twobot.hh
        src/twobot.cc
        src/apiset.cc
        src/jsonex.hh
)


find_package(OpenSSL REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_path(BRYNET_INCLUDE_DIRS "brynet/Version.hpp")
pkg_check_modules(libcoro REQUIRED IMPORTED_TARGET libcoro)

target_include_directories(twobot PRIVATE ${BRYNET_INCLUDE_DIRS})

target_link_libraries(twobot PUBLIC 
    nlohmann_json::nlohmann_json 
    httplib::httplib 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    TBB::tbb TBB::tbbmalloc 
    PkgConfig::libcoro
)
target_compile_definitions(twobot PUBLIC _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING CPPHTTPLIB_OPENSSL_SUPPORT BRYNET_USE_OPENSSL)

add_executable(twobot-demo demo/main.cc)
target_link_libraries(twobot-demo twobot)

target_include_directories(twobot PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>   # for headers when building
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # for client in install mode
)

install(TARGETS twobot
            EXPORT twobot_targets
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(FILES src/twobot.hh DESTINATION include)

install(EXPORT twobot_targets
        FILE twobot-targets.cmake
        NAMESPACE twobot::
        DESTINATION share/twobot)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/twobot-config.cmake"
"include(CMakeFindDependencyMacro)\n"
"find_dependency(OpenSSL REQUIRED)\n"
"find_dependency(httplib CONFIG REQUIRED)\n"
"find_dependency(nlohmann_json CONFIG REQUIRED)\n"
"find_dependency(TBB CONFIG REQUIRED)\n"
"find_dependency(PkgConfig REQUIRED)\n"
"pkg_check_modules(libcoro REQUIRED IMPORTED_TARGET libcoro)\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/twobot-targets.cmake\")\n"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/twobot-config.cmake"
        DESTINATION share/twobot)

if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif()
