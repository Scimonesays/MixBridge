// Sidecar: drains MixBridge Send shared-memory rings and forwards over UDP localhost.
// The audio engine does not ingest this stream yet; use for bring-up and packet capture.

#include "../source/send_shm.h"

#if defined(_WIN32)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#error "mb-send-bridge is Windows-only in this scaffold"
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint16_t kDefaultUdpPort = 47304;
constexpr uint32_t kPacketMagic = 0x4D425344;  // 'MBSD'

#pragma pack(push, 1)
struct UdpAudioPacketHeader {
  uint32_t magic = kPacketMagic;
  uint32_t sequence = 0;
  uint32_t sample_rate = 48000;
  uint16_t frames = 0;
  uint16_t channels = 2;
};
#pragma pack(pop)

void usage() {
  std::fprintf(stderr,
               "Usage: mb-send-bridge --shm Local\\\\MixBridgeSend_<pid>_<instance> [--port 47304]\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::string mapping;
  uint16_t port = kDefaultUdpPort;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--shm") == 0 && i + 1 < argc) {
      mapping = argv[++i];
    } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = static_cast<uint16_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--help") == 0) {
      usage();
      return 0;
    }
  }
  if (mapping.empty()) {
    usage();
    return 1;
  }

  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::fprintf(stderr, "WSAStartup failed\n");
    return 1;
  }

  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::fprintf(stderr, "socket failed\n");
    WSACleanup();
    return 1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  mixbridge_send::SendShmReader reader;
  if (!reader.open(mapping.c_str())) {
    std::fprintf(stderr, "failed to open SHM mapping: %s\n", mapping.c_str());
    closesocket(sock);
    WSACleanup();
    return 1;
  }

  std::fprintf(stderr, "mb-send-bridge draining %s -> udp://127.0.0.1:%u (source=%s)\n", mapping.c_str(),
               port, reader.header()->source_name);

  std::vector<float> scratch(mixbridge_send::kSendRingFrames * mixbridge_send::kSendChannels);
  uint32_t seq = 0;
  uint64_t frames_sent = 0;

  for (;;) {
    const uint32_t got = reader.read_interleaved(scratch.data(), mixbridge_send::kSendRingFrames);
    if (got == 0) {
      Sleep(2);
      continue;
    }

    UdpAudioPacketHeader hdr{};
    hdr.sequence = ++seq;
    hdr.sample_rate = reader.header()->sample_rate;
    hdr.frames = static_cast<uint16_t>(got);
    hdr.channels = mixbridge_send::kSendChannels;

    std::vector<char> packet(sizeof(UdpAudioPacketHeader) +
                             static_cast<size_t>(got) * mixbridge_send::kSendChannels * sizeof(float));
    std::memcpy(packet.data(), &hdr, sizeof(hdr));
    std::memcpy(packet.data() + sizeof(hdr), scratch.data(),
                static_cast<size_t>(got) * mixbridge_send::kSendChannels * sizeof(float));

    sendto(sock, packet.data(), static_cast<int>(packet.size()), 0,
           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    frames_sent += got;
    if ((frames_sent % 48000) < got) {
      std::fprintf(stderr, "forwarded ~%llu s\n", static_cast<unsigned long long>(frames_sent / 48000));
    }
  }
}
