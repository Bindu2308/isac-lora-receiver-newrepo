#pragma once
#include "chirp.h"
#include <cstdint>
#include <vector>

namespace isac_lora {

// Modulates `payload` (arbitrary bytes, e.g. a serialized Fragment) into a
// complete native-rate IQ waveform: [preamble symbols][payload symbols].
// Internally: appends CRC16, whitens, Hamming(4/5)-encodes, diagonally
// interleaves, Gray-encodes, then chirp-modulates each symbol.
//
// NOTE: This is an IMPLICIT-HEADER, FIXED-LENGTH scheme -- the receiver
// must be told `payload.size()` out-of-band (lora_demodulate takes it as a
// parameter) rather than reading it from an in-band header. Real hardware
// interop with RadioLib's explicit-header packets is handled separately in
// the gateway's synchronizer/header-parsing stage (Phase 3+); this
// function's job is only to prove the FEC/interleave/chirp chain is
// internally self-consistent (Phase 1/2).
std::vector<cf32> lora_modulate(const std::vector<uint8_t>& payload, int sf = 7);

// Returns the number of payload symbols (excluding preamble) that
// lora_modulate will produce for a payload of `payload_len` bytes, at the
// given SF. The demodulator needs this to know where the payload ends.
size_t lora_payload_symbol_count(size_t payload_len, int sf = 7);

} // namespace isac_lora
