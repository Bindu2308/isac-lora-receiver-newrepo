#pragma once
#include "../../lora_phy/chirp.h"
#include "../../lora_phy/lora_demodulator.h"
#include <vector>
#include <cstdint>

namespace isac_lora {

struct SyncResult {
    bool success = false;
    size_t raw_packet_start = 0; // sample index into the ORIGINAL oversampled candidate buffer
    int decimation_phase = 0;    // which of the `oversample` phases aligned
    DemodResult demod;           // full decode result once aligned (reuses the validated Phase1/2 chain)
};

// Takes a candidate region of RAW, OVERSAMPLED IQ (e.g. captured at 2 MHz
// for 125 kHz BW -> oversample factor 16, see lora_config.h
// LORA_OVERSAMPLE) and:
//   1. tries each of the `oversample` decimation phases,
//   2. within each phase, slides a native-rate window across the region,
//   3. hands each candidate window to lora_demodulate (the SAME function
//      validated in Phase 1/2) and accepts the first alignment that
//      decodes successfully (preamble matches AND CRC passes).
//
// This deliberately reuses lora_demodulate as the "is this alignment
// correct" test rather than re-implementing preamble correlation
// separately -- one PHY implementation, one source of truth, per spec
// section 48/49.
//
// `expected_payload_len` is fixed at FRAGMENT_WIRE_SIZE (15 bytes) for this
// project since fragments are always exactly that size (see
// common/protocol/fragment.h) -- no in-band header parsing needed.
//
// NOTE: this is a brute-force search, correct but not fast. It's
// appropriate for Phase 3 bring-up (spec section 54: "correctness before
// intelligence/optimization"). If real-time throughput on the Pi becomes a
// bottleneck once this is proven working, replace the search with a
// proper preamble cross-correlation (spec section 13) -- do NOT do that
// optimization before this brute-force version is proven correct on real
// hardware.
SyncResult synchronize(const std::vector<cf32>& raw_oversampled_candidate,
                        int sf,
                        int oversample,
                        size_t expected_payload_len);

} // namespace isac_lora
