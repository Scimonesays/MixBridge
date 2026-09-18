cmake_minimum_required(VERSION 3.24)
project(mixbridge_vst3_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W3 /permissive- /EHsc /MP)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk")
if(NOT EXISTS "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module.h")
  message(FATAL_ERROR "VST3 SDK missing. Run scripts/fetch-vst3sdk.ps1")
endif()

if(NOT TARGET sdk_hosting)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  set(VST_SDK TRUE)
  add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk_build")
endif()

add_library(mixbridge_vst3_scan STATIC src/vst3_scan.cpp)
target_include_directories(mixbridge_vst3_scan PUBLIC include)

add_library(mixbridge_vst3_host STATIC
  src/vst3_processor.cpp
  "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp"
  "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp"
)
target_include_directories(mixbridge_vst3_host PUBLIC include)
target_link_libraries(mixbridge_vst3_host PUBLIC sdk_hosting mixbridge_vst3_scan)
target_link_libraries(mixbridge_vst3_host PRIVATE ole32 uuid shell32 user32)

add_executable(mb-vst3-scan tools/vst3_scan_main.cpp)
target_link_libraries(mb-vst3-scan PRIVATE mixbridge_vst3_scan)

add_executable(mb-vst3-process tools/vst3_process_main.cpp)
target_link_libraries(mb-vst3-process PRIVATE mixbridge_vst3_host)

enable_testing()
add_test(NAME mb-vst3-scan COMMAND mb-vst3-scan)
