#include "mixbridge/engine.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

static int fail(const char* msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  return 1;
}

int main() {
  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) return fail(err.c_str());

  const uint32_t a = engine.add_tone({.hz = 440.0f, .name = "A"}, err);
  const uint32_t b = engine.add_tone({.hz = 1000.0f, .name = "B"}, err);
  if (!a || !b) return fail("add tones");

  if (!engine.set_gain(a, 0.5f)) return fail("gain a");
  if (!engine.set_mute(b, true)) return fail("mute b");

  if (!engine.remove_source(a, err)) return fail(err.c_str());

  auto sources = engine.list_sources();
  if (sources.size() != 1) return fail("expected one source after remove");
  if (sources[0].id != b) return fail("wrong remaining source");
  if (!sources[0].mute) return fail("mute lost on remaining source");

  if (!engine.remove_source(b, err)) return fail("remove b");
  if (!engine.list_sources().empty()) return fail("expected empty");

  // Broadcast cannot go live without destination.
  if (engine.enable_broadcast(err)) return fail("enable_broadcast should fail");
  if (err != "no_live_destination") return fail("expected no_live_destination");
  if (engine.broadcast_state() != mixbridge::BroadcastState::Standby) return fail("broadcast not standby");

  engine.set_live_destination_ready(true);
  // Still needs running/starting engine
  if (engine.enable_broadcast(err)) return fail("enable without running should fail");

  if (!engine.start(err)) return fail(err.c_str());
  // start transitions Starting -> Running on render thread
  for (int i = 0; i < 50 && engine.state() == mixbridge::EngineState::Starting; ++i) {
    Sleep(20);
  }
  if (engine.state() != mixbridge::EngineState::Running) return fail("engine did not reach running");
  if (!engine.enable_broadcast(err)) return fail(err.c_str());
  if (engine.broadcast_state() != mixbridge::BroadcastState::Live) return fail("expected live");
  engine.disable_broadcast();
  if (engine.broadcast_state() != mixbridge::BroadcastState::Standby) return fail("expected standby");

  // Stop must not be required for standby; engine still running.
  if (engine.state() != mixbridge::EngineState::Running) return fail("engine should stay running");

  engine.stop();
  engine.shutdown();
  std::printf("phase41_state_remove_result=PASS\n");
  return 0;
}
