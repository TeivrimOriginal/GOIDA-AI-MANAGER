cmake_minimum_required(VERSION 3.20)
project(GOIDA VERSION 2.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# Options
option(GOIDA_BUILD_TESTS "Build unit tests" ON)
option(GOIDA_USE_HTTPLIB "Enable cpp-httplib support" ON)

# UTF-8 execution charsets for MSVC
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Common definitions
add_compile_definitions(UNICODE _UNICODE _WIN32_WINNT=0x0600 NOMINMAX WIN32_LEAN_AND_MEAN)

# Main executable: GOIDA AI Manager (single-process WIN32)
add_executable(GOIDA WIN32
    src/goida.cpp
    src/sqlite3.c
)

target_include_directories(GOIDA PRIVATE src)

# Link libs: comctl32 winhttp ole32 uuid urlmon ws2_32 for httplib
target_link_libraries(GOIDA PRIVATE
    comctl32
    winhttp
    ole32
    uuid
    urlmon
    ws2_32
    wsock32
)

# Extra flags for MinGW
if(MINGW)
    target_link_options(GOIDA PRIVATE -municode -mwindows)
    target_compile_options(GOIDA PRIVATE -Wall -Wextra)
endif()

# Enable testing
if(GOIDA_BUILD_TESTS)
    enable_testing()

    # Helper to add ctest
    function(goida_add_test name src)
        add_executable(${name} ${src})
        target_include_directories(${name} PRIVATE src)
        target_compile_definitions(${name} PRIVATE UNICODE _UNICODE)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    goida_add_test(test_utf8 tests/test_utf8.cpp)
    goida_add_test(test_i18n tests/test_i18n.cpp)
    goida_add_test(test_json tests/test_json.cpp)
    # test_db needs sqlite3.c
    add_executable(test_db tests/test_db.cpp src/sqlite3.c)
    target_include_directories(test_db PRIVATE src)
    target_compile_definitions(test_db PRIVATE UNICODE _UNICODE)
    add_test(NAME test_db COMMAND test_db)

    # Legacy single test target for compatibility
    if(NOT TARGET test_goida)
        add_executable(test_goida tests/test_json.cpp)
        target_include_directories(test_goida PRIVATE src)
        add_test(NAME unit_json COMMAND test_goida)
    endif()
endif()

# Fallback note for bat builds
# build.bat / build_goida.bat can call: cmake -S . -B build && cmake --build build
# If cmake not available, fallback to g++ direct compile via build.bat

# Install
install(TARGETS GOIDA DESTINATION .)
install(FILES lang/ru.json lang/en.json DESTINATION lang)
