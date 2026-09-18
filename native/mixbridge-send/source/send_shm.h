#pragma once

#include <cstdint>
#include <string>

namespace mixbridge_send {

inline constexpr uint32_t kSendShmMagic = 0x444E534D;  // 'MBSD' little-endian
inline constexpr uint32_t kSendShmVersion = 1;
inline constexpr uint32_t kSendRingFrames = 4096;
inline constexpr uint32_t kSendChannels = 2;
inline constexpr uint32_t kSourceNameMax = 64;

#pragma pack(push, 1)
struct SendShmHeader {
  uint32_t magic = kSendShmMagic;
  uint32_t version = kSendShmVersion;
  uint32_t sample_rate = 48000;
  uint32_t channels = kSendChannels;
  uint32_t capacity_frames = kSendRingFrames;
  char source_name[kSourceNameMax]{};
  volatile uint32_t write_frame = 0;
  volatile uint32_t read_frame = 0;
};
#pragma pack(pop)

class SendShmWriter {
public:
  SendShmWriter() = default;
  ~SendShmWriter();

  SendShmWriter(const SendShmWriter&) = delete;
  SendShmWriter& operator=(const SendShmWriter&) = delete;

  bool open(uint32_t process_id, uint32_t instance_id, const char* source_name, uint32_t sample_rate,
            std::string& mapping_name_out);
  void close();

  bool active() const { return header_ != nullptr; }
  const char* mapping_name() const { return mapping_name_.c_str(); }
  const char* source_name() const;

  // Realtime-safe: interleaved stereo float32, frame count.
  void write_interleaved(const float* interleaved, uint32_t frames);

private:
  void* mapping_ = nullptr;
  SendShmHeader* header_ = nullptr;
  float* ring_ = nullptr;
  std::string mapping_name_;
  uint32_t capacity_frames_ = 0;
};

class SendShmReader {
public:
  SendShmReader() = default;
  ~SendShmReader();

  SendShmReader(const SendShmReader&) = delete;
  SendShmReader& operator=(const SendShmReader&) = delete;

  bool open(const char* mapping_name);
  void close();

  bool active() const { return header_ != nullptr; }
  const SendShmHeader* header() const { return header_; }

  uint32_t read_interleaved(float* interleaved, uint32_t max_frames);

private:
  void* mapping_ = nullptr;
  SendShmHeader* header_ = nullptr;
  float* ring_ = nullptr;
  uint32_t capacity_frames_ = 0;
};

std::string make_mapping_name(uint32_t process_id, uint32_t instance_id);

}  // namespace mixbridge_send
