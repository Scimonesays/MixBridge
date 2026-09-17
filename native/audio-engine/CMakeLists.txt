cmake_minimum_required(VERSION 3.24)
project(mixbridge_audio_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
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

add_executable(mixbridge_audio_engine src/main.cpp)
target_link_libraries(mixbridge_audio_engine PRIVATE mixbridge_engine_core)

add_executable(mb-engine-harness tools/engine_harness.cpp)
target_link_libraries(mb-engine-harness PRIVATE mixbridge_engine_core)

add_executable(mb-engine-soak tools/engine_soak.cpp)
target_link_libraries(mb-engine-soak PRIVATE mixbridge_engine_core)

add_executable(mb-engine-ipc tools/engine_ipc_server.cpp)
target_link_libraries(mb-engine-ipc PRIVATE mixbridge_engine_core)

enable_testing()
add_executable(engine_graph_test tests/graph_test.cpp)
target_link_libraries(engine_graph_test PRIVATE mixbridge_engine_core)
add_test(NAME engine_graph_test COMMAND engine_graph_test)

add_executable(engine_mix_analyze_test tests/mix_analyze_test.cpp)
target_link_libraries(engine_mix_analyze_test PRIVATE mixbridge_engine_core)
add_test(NAME engine_mix_analyze_test COMMAND engine_mix_analyze_test)
