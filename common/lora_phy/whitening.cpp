#include "whitening.h"

namespace isac_lora {

void whiten(uint8_t* data, size_t len) {
    // 9-bit LFSR, poly x^9 + x^5 + 1, seed 0x1FF (all ones).
    // This generates the same class of whitening sequence used in common
    // LoRa software implementations (e.g. gr-lora). Exact bit-for-bit match
    // with a specific vendor's whitening table is NOT required at this
    // stage since TX and RX both live in common/ and use this exact
    // function -- consistency between them is what matters for Phase 1/2.
    uint16_t lfsr = 0x1FF;
    for (size_t i = 0; i < len; ++i) {
        uint8_t whitening_byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            whitening_byte |= static_cast<uint8_t>((lfsr & 0x01) << bit);
            uint8_t newbit = static_cast<uint8_t>((lfsr ^ (lfsr >> 5)) & 0x01);
            lfsr = static_cast<uint16_t>((lfsr >> 1) | (newbit << 8));
        }
        data[i] = static_cast<uint8_t>(data[i] ^ whitening_byte);
    }
}

} // namespace isac_lora
