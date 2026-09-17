#include "mixbridge/ring_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mixbridge {

namespace {
bool is_pow2(std::size_t v) { return v && ((v & (v - 1)) == 0); }
}  // namespace

SpscFloatRing::SpscFloatRing(std::size_t capacity_pow2) {
  if (!is_pow2(capacity_pow2)) {
    throw std::invalid_argument("SpscFloatRing capacity must be power of two");
  }
  capacity_ = capacity_pow2;
  mask_ = capacity_ - 1;
  buf_.assign(capacity_, 0.0f);
}

std::size_t SpscFloatRing::size() const {
  const auto h = head_.load(std::memory_order_acquire);
  const auto t = tail_.load(std::memory_order_acquire);
  return h - t;
}

std::size_t SpscFloatRing::write(const float* data, std::size_t count) {
  const auto h = head_.load(std::memory_order_relaxed);
  const auto t = tail_.load(std::memory_order_acquire);
  const std::size_t free = capacity_ - (h - t);
  const std::size_t n = std::min(count, free);
  for (std::size_t i = 0; i < n; ++i) {
    buf_[(h + i) & mask_] = data[i];
  }
  head_.store(h + n, std::memory_order_release);
  return n;
}

std::size_t SpscFloatRing::read(float* data, std::size_t count) {
  const auto t = tail_.load(std::memory_order_relaxed);
  const auto h = head_.load(std::memory_order_acquire);
  const std::size_t available = h - t;
  const std::size_t n = std::min(count, available);
  for (std::size_t i = 0; i < n; ++i) {
    data[i] = buf_[(t + i) & mask_];
  }
  tail_.store(t + n, std::memory_order_release);
  return n;
}

void SpscFloatRing::clear() {
  tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

}  // namespace mixbridge
