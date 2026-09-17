cmake_minimum_required(VERSION 3.24)
project(mixbridge_probes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(mb-audio-probe
  audio-probe/main.cpp
  audio-probe/wasapi_common.cpp
  audio-probe/enumerate.cpp
  audio-probe/capture_render.cpp
  audio-probe/loopback.cpp
  audio-probe/pcm_analyze.cpp
)
target_include_directories(mb-audio-probe PRIVATE audio-probe)
target_link_libraries(mb-audio-probe PRIVATE
  ole32
  uuid
  mmdevapi
  avrt
  winmm
  mfplat
  mfuuid
  ksuser
  runtimeobject
)

add_executable(mb-tone-player
  tone-player/main.cpp
)
target_link_libraries(mb-tone-player PRIVATE ole32 uuid mmdevapi avrt)

add_executable(mb-pcm-analyze
  audio-probe/pcm_analyze_main.cpp
  audio-probe/pcm_analyze.cpp
  audio-probe/wasapi_common.cpp
)
target_include_directories(mb-pcm-analyze PRIVATE audio-probe)
target_link_libraries(mb-pcm-analyze PRIVATE ole32 uuid)

enable_testing()
add_test(NAME dsp_silence COMMAND mb-pcm-analyze --expect silence --rate 48000 --channels 2 --seconds 0.25 --generate silence)
add_test(NAME dsp_440 COMMAND mb-pcm-analyze --expect tone --freq 440 --rate 48000 --channels 2 --seconds 0.5 --generate tone --tol-db -40)
add_test(NAME dsp_1000 COMMAND mb-pcm-analyze --expect tone --freq 1000 --rate 48000 --channels 2 --seconds 0.5 --generate tone --tol-db -40)
add_test(NAME dsp_impulse COMMAND mb-pcm-analyze --expect impulse --rate 48000 --channels 2 --generate impulse)
