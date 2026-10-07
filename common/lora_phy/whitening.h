#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace isac_lora {

// Whitening scrambles payload bytes with a fixed PN9 sequence so that long
// runs of identical bits don't appear on air. It is its own inverse:
// dewhiten(whiten(x)) == x, using the same sequence.
//
// whiten() XORs `data[0..len)` in place against the PN9 sequence starting
// from seed byte 0. Both TX and RX must use the same sequence, which is why
// this lives in common/ rather than being duplicated per gateway.
void whiten(uint8_t* data, size_t len);
inline void dewhiten(uint8_t* data, size_t len) { whiten(data, len); } // XOR is self-inverse

} // namespace isac_lora
