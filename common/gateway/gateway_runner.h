#pragma once
#include <cstdint>
#include <string>

namespace isac_lora {

// Everything that legitimately differs between Gateway 1 and Gateway 2
// (doc section 23/27): identity, port, gain, and how this gateway gets
// its IQ (live SDR vs offline replay file). Everything else -- signal
// detection, synchronization, PHY decode, observation format -- is one
// shared implementation in common/gateway/ and common/lora_phy/, called
// identically regardless of which config is passed in.
struct GatewayConfig {
    uint8_t gateway_id = 1;
    std::string rpi4_host = "192.168.137.238";
    uint16_t rpi4_port = 5000;
    double gain_db = 15.0;

    // IQ source selection
    bool live_mode = false;      // true = real SDRplay hardware
    std::string replay_file;     // used when live_mode == false
};

// Runs the full gateway pipeline (IQ source -> queue -> detector ->
// synchronizer -> observation -> TCP send) until the IQ source is
// exhausted (replay mode) or interrupted (live mode). Identical code path
// for every gateway -- only `config` differs.
int run_gateway(const GatewayConfig& config);

} // namespace isac_lora
