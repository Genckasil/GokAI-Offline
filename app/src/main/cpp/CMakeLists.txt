cmake_minimum_required(VERSION 3.22.1)

project("gokai_offline")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
        GIT_TAG master
)

FetchContent_MakeAvailable(llama_cpp)

add_library(
        gokai_native
        SHARED
        native-lib.cpp
)

find_library(
        log-lib
        log
)

target_include_directories(
        gokai_native
        PRIVATE
        ${llama_cpp_SOURCE_DIR}/include
)

target_link_libraries(
        gokai_native
        llama
        ${log-lib}
)
