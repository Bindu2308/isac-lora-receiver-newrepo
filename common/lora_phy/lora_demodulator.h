#pragma once
#include "chirp.h"
#include <cstdint>
#include <vector>
#include <string>

namespace isac_lora {

// Per-stage decode result, so a failure always names the exact stage
// (spec section 9 / 47: "never simply report packet decode failed").
enum class DecodeStage {
    OK = 0,
    PREAMBLE_NOT_FOUND,
    PAYLOAD_SYMBOLS_INSUFFICIENT,
    HAMMING_PARITY_FAIL,
    CRC_FAIL,
};

struct DemodResult {
    DecodeStage stage = DecodeStage::PREAMBLE_NOT_FOUND;
    bool ok = false;
    std::vector<uint8_t> payload;      // valid only if ok == true
    float avg_symbol_confidence = 0.0f; // mean of per-symbol chirp-FFT confidence

    const char* stage_name() const {
        switch (stage) {
            case DecodeStage::OK: return "OK";
            case DecodeStage::PREAMBLE_NOT_FOUND: return "PREAMBLE_NOT_FOUND";
            case DecodeStage::PAYLOAD_SYMBOLS_INSUFFICIENT: return "PAYLOAD_SYMBOLS_INSUFFICIENT";
            case DecodeStage::HAMMING_PARITY_FAIL: return "HAMMING_PARITY_FAIL";
            case DecodeStage::CRC_FAIL: return "CRC_FAIL";
        }
        return "UNKNOWN";
    }
};

// Demodulates a native-rate IQ waveform (as produced by lora_modulate) back
// into the original payload bytes. `expected_payload_len` must match what
// the transmitter used (implicit-header scheme, see lora_modulator.h).
//
// Assumes `samples` starts exactly at the first preamble symbol (i.e.
// perfect timing sync) -- real-world timing/frequency synchronization
// against noisy, offset-in-time RF captures is the gateway synchronizer's
// job (Phase 3+), not this function's. This function's only job is to
// prove the FEC/interleave/chirp chain round-trips correctly.
DemodResult lora_demodulate(const std::vector<cf32>& samples,
                             size_t expected_payload_len,
                             int sf = 7);

} // namespace isac_lora
