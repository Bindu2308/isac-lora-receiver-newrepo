#include "signal_detector.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace isac_lora {

namespace {
float power_db_of(const std::vector<float>& mags, size_t start, size_t len) {
    float sum_sq = 0.0f;
    for (size_t i = start; i < start + len && i < mags.size(); ++i) {
        sum_sq += mags[i] * mags[i];
    }
    float mean_sq = sum_sq / static_cast<float>(len);
    return 10.0f * std::log10(std::max(mean_sq, 1e-12f));
}
} // namespace

std::vector<Candidate> SignalDetector::detect(const std::vector<cf32>& samples,
                                               size_t window_size) {
    std::vector<Candidate> candidates;
    if (samples.size() < window_size) return candidates;

    // per-sample magnitude
    std::vector<float> mags(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) mags[i] = std::abs(samples[i]);

    // windowed power estimate
    size_t num_windows = samples.size() / window_size;
    std::vector<float> window_power_db(num_windows);
    for (size_t w = 0; w < num_windows; ++w) {
        window_power_db[w] = power_db_of(mags, w * window_size, window_size);
    }

    // noise floor = 15th-percentile window power. A low percentile (rather
    // than the median) stays robust even if a capture happens to have a
    // higher signal duty cycle than the ideal "mostly quiet channel"
    // assumption -- median breaks down as soon as >50% of windows contain
    // signal, which is exactly the failure mode caught by this project's
    // own synthetic offline test.
    std::vector<float> sorted_power = window_power_db;
    std::sort(sorted_power.begin(), sorted_power.end());
    size_t percentile_idx = sorted_power.empty() ? 0 : (sorted_power.size() * 15) / 100;
    float noise_floor_db = sorted_power.empty() ? -100.0f : sorted_power[percentile_idx];

    // find contiguous windows above threshold
    bool in_region = false;
    size_t region_start_window = 0;
    for (size_t w = 0; w < num_windows; ++w) {
        bool above = (window_power_db[w] - noise_floor_db) >= threshold_db_;
        if (above && !in_region) {
            in_region = true;
            region_start_window = w;
        } else if (!above && in_region) {
            in_region = false;
            Candidate c;
            c.start_sample = region_start_window * window_size;
            c.end_sample = w * window_size;
            float region_max_power = *std::max_element(
                window_power_db.begin() + static_cast<long>(region_start_window),
                window_power_db.begin() + static_cast<long>(w));
            c.power_db = region_max_power;
            c.noise_db = noise_floor_db;
            c.confidence = region_max_power - noise_floor_db;
            candidates.push_back(c);
        }
    }
    if (in_region) {
        Candidate c;
        c.start_sample = region_start_window * window_size;
        c.end_sample = num_windows * window_size;
        float region_max_power = *std::max_element(
            window_power_db.begin() + static_cast<long>(region_start_window),
            window_power_db.end());
        c.power_db = region_max_power;
        c.noise_db = noise_floor_db;
        c.confidence = region_max_power - noise_floor_db;
        candidates.push_back(c);
    }

    return candidates;
}

} // namespace isac_lora
