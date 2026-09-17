cmake_minimum_required(VERSION 3.24)
project(mixbridge_vst3_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_library(mixbridge_vst3_scan STATIC src/vst3_scan.cpp)
target_include_directories(mixbridge_vst3_scan PUBLIC include)

add_executable(mb-vst3-scan tools/vst3_scan_main.cpp)
target_link_libraries(mb-vst3-scan PRIVATE mixbridge_vst3_scan)

enable_testing()
add_test(NAME mb-vst3-scan COMMAND mb-vst3-scan)
