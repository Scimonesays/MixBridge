cmake_minimum_required(VERSION 3.24)
project(mixbridge_audio_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_library(mixbridge_engine_core STATIC
  src/ring_buffer.cpp
  src/mix_graph.cpp
  src/engine.cpp
  src/device_manager.cpp
  src/wasapi_util.cpp
  src/wasapi_io.cpp
)
target_include_directories(mixbridge_engine_core PUBLIC include)
target_link_libraries(mixbridge_engine_core PUBLIC
  ole32 uuid mmdevapi avrt ksuser runtimeobject
)

set(MIXBRIDGE_VST3_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk"
    CACHE PATH "Path to the pinned Steinberg VST3 SDK checkout")
if(EXISTS "${MIXBRIDGE_VST3_SDK}/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../vst3-host"
                   "${CMAKE_BINARY_DIR}/vst3-host" EXCLUDE_FROM_ALL)
  target_sources(mixbridge_engine_core PRIVATE src/vst3_effect_factory.cpp)
  target_link_libraries(mixbridge_engine_core PRIVATE mixbridge_vst3_host)
  target_compile_definitions(mixbridge_engine_core PRIVATE MIXBRIDGE_HAS_VST3=1)
else()
  target_sources(mixbridge_engine_core PRIVATE src/vst3_effect_stub.cpp)
  target_compile_definitions(mixbridge_engine_core PRIVATE MIXBRIDGE_HAS_VST3=0)
endif()

add_executable(mixbridge_audio_engine src/main.cpp)
target_link_libraries(mixbridge_audio_engine PRIVATE mixbridge_engine_core)

add_executable(mb-engine-harness tools/engine_harness.cpp)
target_link_libraries(mb-engine-harness PRIVATE mixbridge_engine_core)

add_executable(mb-engine-soak tools/engine_soak.cpp)
target_link_libraries(mb-engine-soak PRIVATE mixbridge_engine_core)

add_executable(mb-engine-ipc tools/engine_ipc_server.cpp)
target_link_libraries(mb-engine-ipc PRIVATE mixbridge_engine_core)
if(WIN32)
  target_link_libraries(mb-engine-ipc PRIVATE user32)
endif()

enable_testing()
add_executable(engine_graph_test tests/graph_test.cpp)
target_link_libraries(engine_graph_test PRIVATE mixbridge_engine_core)
add_test(NAME engine_graph_test COMMAND engine_graph_test)

add_executable(engine_mix_analyze_test tests/mix_analyze_test.cpp)
target_link_libraries(engine_mix_analyze_test PRIVATE mixbridge_engine_core)
add_test(NAME engine_mix_analyze_test COMMAND engine_mix_analyze_test)

add_executable(engine_phase41_state_test tests/phase41_state_test.cpp)
target_link_libraries(engine_phase41_state_test PRIVATE mixbridge_engine_core)
add_test(NAME engine_phase41_state_test COMMAND engine_phase41_state_test)
