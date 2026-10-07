#include "chirp.h"
#include <cmath>
#include <numeric>

namespace isac_lora {

namespace {

constexpr float PI_F = 3.14159265358979323846f;

// In-place iterative radix-2 Cooley-Tukey FFT. `data.size()` must be a
// power of two (always true here: 2^sf). Fast enough for SF7..SF12 in
// real time on a Pi -- avoids the O(n^2) naive DFT the spec explicitly
// calls out as a known Gateway 2 bug.
void fft_inplace(std::vector<cf32>& data) {
    size_t n = data.size();
    if (n <= 1) return;

    // bit-reversal permutation
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * PI_F / static_cast<float>(len);
        cf32 wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cf32 w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                cf32 u = data[i + k];
                cf32 v = data[i + k + len / 2] * w;
                data[i + k] = u + v;
                data[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

} // namespace

std::vector<cf32> base_upchirp(int sf) {
    int n = 1 << sf; // samples per symbol at native rate
    std::vector<cf32> chirp(static_cast<size_t>(n));
    // Instantaneous frequency ramps from -BW/2 to +BW/2 over the symbol;
    // in normalized (native-rate) terms this is phase(t) = pi*t^2/n - pi*t.
    for (int t = 0; t < n; ++t) {
        float phase = PI_F * static_cast<float>(t) * static_cast<float>(t) / static_cast<float>(n)
                    - PI_F * static_cast<float>(t);
        chirp[static_cast<size_t>(t)] = cf32(std::cos(phase), std::sin(phase));
    }
    return chirp;
}

std::vector<cf32> base_downchirp(int sf) {
    auto up = base_upchirp(sf);
    for (auto& s : up) s = std::conj(s);
    return up;
}

std::vector<cf32> modulate_symbol(int sf, uint16_t symbol) {
    int n = 1 << sf;
    auto up = base_upchirp(sf);
    std::vector<cf32> out(static_cast<size_t>(n));
    // Cyclic shift by `symbol` samples: this is what encodes the symbol
    // value onto the chirp waveform.
    for (int t = 0; t < n; ++t) {
        out[static_cast<size_t>(t)] = up[static_cast<size_t>((t + symbol) % n)];
    }
    return out;
}

uint16_t demodulate_symbol(int sf, const std::vector<cf32>& samples, float* confidence) {
    int n = 1 << sf;
    auto down = base_downchirp(sf);
    std::vector<cf32> dechirped(static_cast<size_t>(n));
    for (int t = 0; t < n; ++t) {
        dechirped[static_cast<size_t>(t)] = samples[static_cast<size_t>(t)] * down[static_cast<size_t>(t)];
    }
    fft_inplace(dechirped);

    // find peak magnitude bin
    size_t peak_bin = 0;
    float peak_mag = -1.0f;
    float mag_sum = 0.0f;
    for (size_t k = 0; k < dechirped.size(); ++k) {
        float mag = std::abs(dechirped[k]);
        mag_sum += mag;
        if (mag > peak_mag) {
            peak_mag = mag;
            peak_bin = k;
        }
    }
    if (confidence) {
        float mean_mag = mag_sum / static_cast<float>(dechirped.size());
        *confidence = (mean_mag > 0.0f) ? (peak_mag / mean_mag) : 0.0f;
    }
    return static_cast<uint16_t>(peak_bin);
}

uint16_t gray_encode(uint16_t value) {
    return static_cast<uint16_t>(value ^ (value >> 1));
}

uint16_t gray_decode(uint16_t value) {
    uint16_t mask = value;
    while (mask) {
        mask >>= 1;
        value ^= mask;
    }
    return value;
}

} // namespace isac_lora
