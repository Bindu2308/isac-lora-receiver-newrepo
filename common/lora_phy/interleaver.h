#pragma once
#include <cstdint>
#include <vector>

namespace isac_lora {

// LoRa's diagonal interleaver operates on blocks of SF Hamming codewords
// (each `cr_plus4` bits wide, e.g. 5 bits at CR 4/5) and produces a block of
// `cr_plus4` symbols (each SF bits wide) via a diagonal bit permutation.
// This spreads a single bad codeword's bit errors across multiple symbols,
// which is why deinterleaving must exactly invert this before Hamming
// decoding.
//
// interleave: input.size() must be a multiple of sf; each input element
// uses its low `cr_plus4` bits. Output.size() = input.size() / sf * cr_plus4;
// each output element uses its low `sf` bits.
std::vector<uint16_t> interleave_block(const std::vector<uint8_t>& codewords,
                                        int sf, int cr_plus4);

// deinterleave: exact inverse of interleave_block.
std::vector<uint8_t> deinterleave_block(const std::vector<uint16_t>& symbols,
                                         int sf, int cr_plus4);

} // namespace isac_lora
