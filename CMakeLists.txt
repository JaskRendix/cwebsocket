cmake_minimum_required(VERSION 3.10)
project(ModernCWebsocket C)

# Use C11 for better type safety and modern features
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable aggressive warnings
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
    )
endif()

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

if (ENABLE_SANITIZERS)
    message(STATUS "Sanitizers enabled")
    set(SAN_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SAN_FLAGS}")
endif()

# Library sources
set(CWEBSOCKET_SOURCES
    lib/websocket.c
)

# Create the library
add_library(cwebsocket STATIC ${CWEBSOCKET_SOURCES})
target_include_directories(cwebsocket PUBLIC lib)

# Tests
option(CWEBSOCKET_BUILD_TESTS "Build unit tests" ON)

if (CWEBSOCKET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
