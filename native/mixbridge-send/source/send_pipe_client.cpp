#include "send_pipe_client.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstdio>
#include <sstream>

namespace mixbridge_send {

namespace {

bool write_line(HANDLE pipe, const std::string& line) {
  std::string payload = line;
  payload.push_back('\n');
  DWORD written = 0;
  return WriteFile(pipe, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) != 0;
}

bool read_line(HANDLE pipe, std::string& out) {
  out.clear();
  char c = 0;
  DWORD read = 0;
  while (ReadFile(pipe, &c, 1, &read, nullptr) && read == 1) {
    if (c == '\n') return true;
    if (c != '\r') out.push_back(c);
    if (out.size() > 8192) return false;
  }
  return false;
}

}  // namespace

bool register_vst3_send(const std::string& source_name, const std::string& shm_mapping_name,
                        std::string& response_out) {
  response_out.clear();
#if !defined(_WIN32)
  response_out = "unsupported_platform";
  return false;
#else
  HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\mixbridge-engine", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                          OPEN_EXISTING, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    response_out = "engine_pipe_unavailable";
    return false;
  }

  std::string hello;
  if (!read_line(pipe, hello)) {
    CloseHandle(pipe);
    response_out = "hello_failed";
    return false;
  }

  std::ostringstream cmd;
  cmd << "REGISTER_VST3_SEND " << source_name << " SHM " << shm_mapping_name;
  if (!write_line(pipe, cmd.str())) {
    CloseHandle(pipe);
    response_out = "write_failed";
    return false;
  }

  if (!read_line(pipe, response_out)) {
    CloseHandle(pipe);
    response_out = "response_failed";
    return false;
  }

  CloseHandle(pipe);
  return response_out.rfind("OK", 0) == 0;
#endif
}

}  // namespace mixbridge_send
