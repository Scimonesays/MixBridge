#include "mixbridge/engine.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

  const uint32_t keys =
    engine.add_starter_instrument({.preset = 0, .name = "Neon Keys"}, err);
  if (!keys) return fail("add starter instrument");
  if (!engine.instrument_note_on(keys, 60, 0.9f, err)) return fail("instrument note on");
  std::vector<float> synth_mon(512 * 2, 0.0f);
  std::vector<float> synth_bc(512 * 2, 0.0f);
  engine.render_offline(512, synth_mon.data(), synth_bc.data());
  float synth_peak = 0.0f;
  for (float sample : synth_mon) synth_peak = std::max(synth_peak, std::abs(sample));
  if (synth_peak < 0.001f) return fail("starter instrument produced silence");
  if (!engine.instrument_note_off(keys, 60, err)) return fail("instrument note off");
  if (!engine.instrument_all_notes_off(keys, err)) return fail("instrument notes off");
  if (!engine.remove_source(keys, err)) return fail("remove instrument");
  if (!engine.list_sources().empty()) return fail("instrument remove failed");

  // Broadcast cannot go live without destination.
  if (engine.enable_broadcast(err)) return fail("enable_broadcast should fail");
  if (err != "no_live_destination") return fail("expected no_live_destination");
  if (engine.broadcast_state() != mixbridge::BroadcastState::Standby) return fail("broadcast not standby");

  // Hosted CI runners may have no render endpoint at all. Keep the hardware-free
  // state/source assertions in CI; the RME acceptance harness owns the real
  // monitor + live transition proof.
  if (engine.list_render_devices().empty()) {
    engine.shutdown();
    std::printf("phase41_state_remove_result=PASS hardware_live=SKIP_no_render_endpoint\n");
    return 0;
  }

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
