// gateway2/src/main.cpp
//
// Gateway 2 entry point. Deliberately near-identical to gateway1/main.cpp:
// only gateway_id, rpi4_port, and gain differ, per architecture doc
// section 23. All receive logic is the shared common/gateway/gateway_runner
// -- Gateway 2 does NOT get its own copy of the demodulator/synchronizer.
//
// Usage:
//   ./gateway2 --replay <file.cf32>     offline, no hardware needed
//   ./gateway2 --live                   real SDRplay RSP1B (Pi + hardware)

#include "../../common/gateway/gateway_runner.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace isac_lora;

int main(int argc, char** argv) {
    GatewayConfig config;
    config.gateway_id = 2;
    config.rpi4_host = "192.168.137.238"; // update if your RPi4's IP changes
    config.rpi4_port = 5001;
    config.gain_db = 20.0; // STARTING POINT ONLY, NOT CONFIRMED FOR THIS CODEBASE:
                            // your prior Gateway 2 notes used the SDRplay gRdB
                            // convention (IF gain REDUCTION -- lower gRdB = more
                            // signal = more overload risk; gRdB=40/LNAstate=7 got
                            // overload near zero there). SoapySDR's generic "gain"
                            // parameter may map differently depending on driver
                            // version. Treat this value as a starting point and
                            // retune against real RF once on hardware -- don't
                            // assume it's equivalent to the old gRdB=40 setting.

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
