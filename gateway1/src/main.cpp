// gateway1/src/main.cpp
//
// Gateway 1 entry point. This file must contain ONLY configuration that is
// legitimately allowed to differ between Gateway 1 and Gateway 2
// (gateway_id, TCP port, gain, physical/location-specific settings) --
// see architecture doc section 23. All actual receive logic (detection,
// synchronization, PHY decode, observation building, TCP send) lives in
// common/gateway/gateway_runner.cpp and is shared verbatim with Gateway 2.
//
// Usage:
//   ./gateway1 --replay <file.cf32>     offline, no hardware needed
//   ./gateway1 --live                   real SDRplay RSP1B (Pi + hardware)

#include "../../common/gateway/gateway_runner.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace isac_lora;

int main(int argc, char** argv) {
    GatewayConfig config;
    config.gateway_id = 1;
    config.rpi4_host = "192.168.137.238"; // update if your RPi4's IP changes
    config.rpi4_port = 5000;
    config.gain_db = 15.0; // matches Gateway 1's previously confirmed working gain

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            config.live_mode = false;
            config.replay_file = argv[++i];
        } else if (std::strcmp(argv[i], "--live") == 0) {
            config.live_mode = true;
        } else if (std::strcmp(argv[i], "--rpi4-host") == 0 && i + 1 < argc) {
            config.rpi4_host = argv[++i];
        } else if (std::strcmp(argv[i], "--rpi4-port") == 0 && i + 1 < argc) {
            config.rpi4_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
    }
    if (!config.live_mode && config.replay_file.empty()) {
        std::fprintf(stderr, "Usage: %s --replay <file.cf32> | --live\n", argv[0]);
        return 1;
    }

    return run_gateway(config);
}
