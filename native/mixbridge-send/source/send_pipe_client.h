#pragma once

#include <string>

namespace mixbridge_send {

// Control-thread helper: register a send mapping with mb-engine-ipc (best effort).
bool register_vst3_send(const std::string& source_name, const std::string& shm_mapping_name,
                        std::string& response_out);

}  // namespace mixbridge_send
