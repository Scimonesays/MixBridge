#include "send_shm.h"

#if defined(_WIN32)
#include <windows.h>
#else
#error "MixBridge Send SHM is Windows-only in this scaffold"
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace mixbridge_send {

namespace {

std::size_t mapping_bytes(uint32_t capacity_frames) {
  return sizeof(SendShmHeader) + static_cast<std::size_t>(capacity_frames) * kSendChannels * sizeof(float);
}

void copy_name(char* dst, const char* src) {
  if (!dst) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  std::strncpy(dst, src, kSourceNameMax - 1);
  dst[kSourceNameMax - 1] = '\0';
}

}  // namespace

std::string make_mapping_name(uint32_t process_id, uint32_t instance_id) {
  char buf[128];
  std::snprintf(buf, sizeof(buf), "Local\\MixBridgeSend_%u_%u", process_id, instance_id);
  return buf;
}

SendShmWriter::~SendShmWriter() { close(); }

bool SendShmWriter::open(uint32_t process_id, uint32_t instance_id, const char* source_name,
                         uint32_t sample_rate, std::string& mapping_name_out) {
  close();
  mapping_name_ = make_mapping_name(process_id, instance_id);
  capacity_frames_ = kSendRingFrames;
  const std::size_t bytes = mapping_bytes(capacity_frames_);

  mapping_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                static_cast<DWORD>(bytes), mapping_name_.c_str());
  if (!mapping_) return false;

  header_ = static_cast<SendShmHeader*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, bytes));
  if (!header_) {
    close();
    return false;
  }

  header_->magic = kSendShmMagic;
  header_->version = kSendShmVersion;
  header_->sample_rate = sample_rate;
  header_->channels = kSendChannels;
  header_->capacity_frames = capacity_frames_;
  copy_name(header_->source_name, source_name);
  header_->write_frame = 0;
  header_->read_frame = 0;

  ring_ = reinterpret_cast<float*>(header_ + 1);
  mapping_name_out = mapping_name_;
  return true;
}

void SendShmWriter::close() {
  if (header_) {
    UnmapViewOfFile(header_);
    header_ = nullptr;
    ring_ = nullptr;
  }
  if (mapping_) {
    CloseHandle(static_cast<HANDLE>(mapping_));
    mapping_ = nullptr;
  }
  mapping_name_.clear();
  capacity_frames_ = 0;
}

const char* SendShmWriter::source_name() const {
  return header_ ? header_->source_name : "";
}

void SendShmWriter::write_interleaved(const float* interleaved, uint32_t frames) {
  if (!header_ || !ring_ || !interleaved || frames == 0) return;

  const uint32_t cap = capacity_frames_;
  uint32_t w = header_->write_frame;
  const uint32_t r = header_->read_frame;
  uint32_t used = w - r;
  if (used > cap) used = cap;

  for (uint32_t i = 0; i < frames; ++i) {
    if (used >= cap) {
      // Drop newest samples when the sidecar is not draining.
      break;
    }
    const uint32_t idx = (w % cap) * kSendChannels;
    ring_[idx] = interleaved[i * kSendChannels];
    ring_[idx + 1] = interleaved[i * kSendChannels + 1];
    ++w;
    ++used;
  }
  header_->write_frame = w;
}

SendShmReader::~SendShmReader() { close(); }

bool SendShmReader::open(const char* mapping_name) {
  close();
  if (!mapping_name || !mapping_name[0]) return false;

  mapping_ = OpenFileMappingA(FILE_MAP_READ, FALSE, mapping_name);
  if (!mapping_) return false;

  header_ = static_cast<SendShmHeader*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
  if (!header_ || header_->magic != kSendShmMagic || header_->version != kSendShmVersion) {
    close();
    return false;
  }

  capacity_frames_ = header_->capacity_frames;
  ring_ = reinterpret_cast<float*>(header_ + 1);
  return true;
}

void SendShmReader::close() {
  if (header_) {
    UnmapViewOfFile(header_);
    header_ = nullptr;
    ring_ = nullptr;
  }
  if (mapping_) {
    CloseHandle(static_cast<HANDLE>(mapping_));
    mapping_ = nullptr;
  }
  capacity_frames_ = 0;
}

uint32_t SendShmReader::read_interleaved(float* interleaved, uint32_t max_frames) {
  if (!header_ || !ring_ || !interleaved || max_frames == 0) return 0;

  const uint32_t cap = capacity_frames_;
  uint32_t r = header_->read_frame;
  const uint32_t w = header_->write_frame;
  uint32_t avail = w - r;
  if (avail > cap) avail = cap;
  const uint32_t to_read = std::min(avail, max_frames);

  for (uint32_t i = 0; i < to_read; ++i) {
    const uint32_t idx = (r % cap) * kSendChannels;
    interleaved[i * kSendChannels] = ring_[idx];
    interleaved[i * kSendChannels + 1] = ring_[idx + 1];
    ++r;
  }
  header_->read_frame = r;
  return to_read;
}

}  // namespace mixbridge_send
