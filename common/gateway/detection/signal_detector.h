#pragma once
#include "../../lora_phy/chirp.h"
#include <vector>
#include <cstdint>

namespace isac_lora {

struct Candidate {
    size_t start_sample = 0;
    size_t end_sample = 0;
    float power_db = 0.0f;
    float noise_db = 0.0f;
    float confidence = 0.0f; // (power_db - noise_db), simple SNR-like margin
};

class SignalDetector {
public:
    // `threshold_db` = how many dB above the estimated noise floor a region
    // must be to count as a candidate. Start conservative (e.g. 6-10 dB)
    // and tune against real captures once on hardware.
    explicit SignalDetector(float threshold_db = 8.0f) : threshold_db_(threshold_db) {}

    // Scans `samples` for contiguous above-threshold regions. Noise floor
    // is estimated as the median power over a sliding window -- robust to
    // short bursts of signal not dominating the estimate. Returns one
    // Candidate per contiguous region found; logs nothing itself (caller
    // logs per spec section 34 with whatever count/positions this returns).
    std::vector<Candidate> detect(const std::vector<cf32>& samples,
                                   size_t window_size = 512);

private:
    float threshold_db_;
};

} // namespace isac_lora
