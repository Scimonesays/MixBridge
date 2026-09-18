cmake_minimum_required(VERSION 3.24)
project(mixbridge_send VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W3 /permissive- /EHsc /MP)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk")
if(NOT EXISTS "${VST3_SDK_ROOT}/public.sdk/source/vst/vstaudioeffect.h")
  message(FATAL_ERROR "VST3 SDK missing. Run scripts/fetch-vst3sdk.ps1")
endif()

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
include(SMTG_VST3_SDK)

if(NOT TARGET sdk)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
  set(VST_SDK TRUE)
  add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk_build")
endif()

smtg_add_vst3plugin(mixbridge-send
  source/mixbridge_send_ids.h
  source/mixbridge_send_version.h
  source/send_shm.cpp
  source/send_shm.h
  source/send_pipe_client.cpp
  source/send_pipe_client.h
  source/mixbridge_send_processor.cpp
  source/mixbridge_send_processor.h
  source/mixbridge_send_controller.cpp
  source/mixbridge_send_controller.h
  source/mixbridge_send_factory.cpp
)

smtg_target_configure_version_file(mixbridge-send)

if(WIN32)
  target_sources(mixbridge-send PRIVATE resource/info.rc)
endif()

target_link_libraries(mixbridge-send PRIVATE sdk ws2_32)

target_compile_features(mixbridge-send PUBLIC cxx_std_17)

add_executable(mb-send-bridge tools/mb_send_bridge.cpp source/send_shm.cpp)
target_include_directories(mb-send-bridge PRIVATE source)
target_link_libraries(mb-send-bridge PRIVATE ws2_32)
if(MSVC)
  target_compile_options(mb-send-bridge PRIVATE /W3 /EHsc)
  target_compile_definitions(mb-send-bridge PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
