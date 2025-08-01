cmake_minimum_required(VERSION 3.1)
project(rrr LANGUAGES C CXX)

set(ABC_SKIP_TESTS true)
add_compile_options(-O3)

add_subdirectory(lib)

option(USE_MOCKTURTLE "Enable mockturtle" ON)  
if(USE_MOCKTURTLE)
  add_subdirectory(mockturtle)
  set(ABC_USE_NAMESPACE "aabbcc")
endif()

add_subdirectory(abc)

include_directories(${PROJECT_SOURCE_DIR}/src)

#file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
#add_executable(rrr ${SRC_FILES} ${PROJECT_SOURCE_DIR}/src/main.c)

add_executable(rrr ${PROJECT_SOURCE_DIR}/src/application_c/main.cpp ${PROJECT_SOURCE_DIR}/src/application_c/giaRrr.cpp ${PROJECT_SOURCE_DIR}/src/interface/rrrMockturtle.cpp)
target_compile_options(rrr PRIVATE -g -Wall -Wextra)
if(USE_MOCKTURTLE)
  target_compile_definitions(rrr PRIVATE USE_MOCKTURTLE)
  target_link_libraries(rrr libabc mockturtle)
else()
  target_link_libraries(rrr libabc)
endif()

file(GLOB FILENAMES ${PROJECT_SOURCE_DIR}/src/application/*.cpp)
foreach(filename ${FILENAMES})
  get_filename_component(basename ${filename} NAME_WE)
  add_executable(${basename} ${filename} ${PROJECT_SOURCE_DIR}/src/interface/rrrMockturtle.cpp)
  target_compile_options(${basename} PRIVATE -g -Wall -Wextra)
  if(USE_MOCKTURTLE)
    target_compile_definitions(${basename} PRIVATE USE_MOCKTURTLE)
    target_link_libraries(${basename} cxxopts libabc mockturtle)
  else()
    target_link_libraries(${basename} cxxopts libabc)
  endif()
endforeach()



