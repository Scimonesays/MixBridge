#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mixbridge {

// Single-producer single-consumer float ring. Capacity must be power of two.
class SpscFloatRing {
public:
  explicit SpscFloatRing(std::size_t capacity_pow2);

  std::size_t capacity() const { return capacity_; }
  std::size_t size() const;
  std::size_t write(const float* data, std::size_t count);
  std::size_t read(float* data, std::size_t count);
  void clear();

private:
  std::vector<float> buf_;
  std::size_t capacity_ = 0;
  std::size_t mask_ = 0;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

}  // namespace mixbridge
