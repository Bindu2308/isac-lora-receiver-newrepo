#pragma once
#include <complex>
#include <vector>
#include <cstdint>

namespace isac_lora {

using cf32 = std::complex<float>;

// All chirp generation here operates at the NATIVE rate, i.e. Fs == BW,
// so one symbol = 2^SF samples exactly (see lora_config.h LORA_NATIVE_SPS).
// Gateways capturing at a higher rate (e.g. 2 MHz for 125 kHz BW) must
// decimate/resample down to native rate before using these functions --
// that resampling step lives in the gateway's synchronizer, not here, so
// this library stays hardware-rate-agnostic and independently testable.

// Generates the canonical LoRa base upchirp for the given spreading factor:
// a complex sinusoid whose instantaneous frequency ramps linearly from
// -BW/2 to +BW/2 over one symbol period, sampled at Fs=BW (native rate).
std::vector<cf32> base_upchirp(int sf);

// Base downchirp = complex conjugate of the base upchirp.
std::vector<cf32> base_downchirp(int sf);

// Modulates a single symbol value (0 .. 2^sf - 1) onto a chirp: cyclically
// shifts the base upchirp by `symbol` samples. This is what gets
// transmitted for one LoRa symbol.
std::vector<cf32> modulate_symbol(int sf, uint16_t symbol);

// Demodulates one symbol's worth of native-rate IQ samples (length 2^sf)
// back to a symbol value via dechirp + FFT peak detection.
// Returns the detected symbol (0..2^sf-1). `confidence` (if non-null) is
// set to the ratio of peak FFT magnitude to the mean magnitude of all bins
// -- a simple, cheap confidence metric usable for soft-information later.
uint16_t demodulate_symbol(int sf, const std::vector<cf32>& samples, float* confidence = nullptr);

// Gray encode/decode, applied to symbol values before/after chirp
// modulation per the LoRa PHY spec.
uint16_t gray_encode(uint16_t value);
uint16_t gray_decode(uint16_t value);

} // namespace isac_lora
