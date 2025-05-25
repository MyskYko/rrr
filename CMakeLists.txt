cmake_minimum_required(VERSION 3.1)
project(rrr LANGUAGES C CXX)

set(ABC_SKIP_TESTS true)
add_compile_options(-O3)

add_subdirectory(mockturtle)
set(ABC_USE_NAMESPACE "aabbcc")
add_subdirectory(abc)

include_directories(${PROJECT_SOURCE_DIR}/src)
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(rrr ${SRC_FILES})
#add_executable(rrr ${SRC_FILES} ${PROJECT_SOURCE_DIR}/src/main.c)

target_link_libraries(rrr libabc mockturtle)

target_compile_options(rrr PRIVATE -g -Wall -Wextra)
