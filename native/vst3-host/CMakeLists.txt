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

set(MIXBRIDGE_VST3_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk"
    CACHE PATH "Path to the pinned Steinberg VST3 SDK checkout")

if(EXISTS "${MIXBRIDGE_VST3_SDK}/CMakeLists.txt")
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  add_subdirectory("${MIXBRIDGE_VST3_SDK}" "${CMAKE_BINARY_DIR}/steinberg-vst3sdk" EXCLUDE_FROM_ALL)

  add_library(mixbridge_vst3_host STATIC
    src/vst3_host.cpp
    "${MIXBRIDGE_VST3_SDK}/public.sdk/source/vst/hosting/plugprovider.cpp"
  )
  if(WIN32)
    target_sources(mixbridge_vst3_host PRIVATE
      "${MIXBRIDGE_VST3_SDK}/public.sdk/source/vst/hosting/module_win32.cpp"
    )
  elseif(APPLE)
    target_sources(mixbridge_vst3_host PRIVATE
      "${MIXBRIDGE_VST3_SDK}/public.sdk/source/vst/hosting/module_mac.mm"
    )
  else()
    target_sources(mixbridge_vst3_host PRIVATE
      "${MIXBRIDGE_VST3_SDK}/public.sdk/source/vst/hosting/module_linux.cpp"
    )
  endif()
  target_include_directories(mixbridge_vst3_host
    PUBLIC include
    PRIVATE "${MIXBRIDGE_VST3_SDK}")
  target_link_libraries(mixbridge_vst3_host PRIVATE sdk_hosting)

  add_executable(mb-vst3-probe tools/vst3_probe_main.cpp)
  target_link_libraries(mb-vst3-probe PRIVATE mixbridge_vst3_host)

  option(MIXBRIDGE_BUILD_VST3_FIXTURE "Build Steinberg AGain fixture for host verification" OFF)
  if(MIXBRIDGE_BUILD_VST3_FIXTURE)
    add_subdirectory(
      "${MIXBRIDGE_VST3_SDK}/public.sdk/samples/vst/again_sampleaccurate"
      "${CMAKE_BINARY_DIR}/mixbridge-vst3-fixture"
      EXCLUDE_FROM_ALL
    )
  endif()
else()
  message(STATUS "MixBridge VST3 SDK not present; scanner-only build. Run scripts/fetch-vst3sdk.ps1.")
endif()

enable_testing()
add_test(NAME mb-vst3-scan COMMAND mb-vst3-scan)
