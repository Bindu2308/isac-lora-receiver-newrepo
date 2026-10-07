#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace isac_lora {

// LoRa FEC operates on 4-bit nibbles. At coding rate 4/5 (the only rate this
// codebase supports, matching the existing gateway implementation), each
// 4-bit data nibble is encoded into a 5-bit codeword by appending one
// even-parity bit. This gives single-bit ERROR DETECTION only (no
// correction) -- consistent with what's already implemented on Gateway 1.
//
// Encoding a full byte produces two 5-bit codewords (high nibble, low
// nibble), returned packed as described in interleaver.h.

// Encode: input = N data bytes: output = 2*N codewords, one per nibble,
// each codeword stored in the low 5 bits of a uint8_t.
std::vector<uint8_t> hamming_encode(const std::vector<uint8_t>& data);

// Decode: input = codewords (low 5 bits each, as produced by hamming_encode
// or reconstructed from the deinterleaver). Output = original data bytes.
// `ok` is set false if any codeword fails its parity check (bit was flipped
// and cannot be corrected at this rate) -- caller should treat as CRC-fail.
std::vector<uint8_t> hamming_decode(const std::vector<uint8_t>& codewords, bool* ok);

} // namespace isac_lora
