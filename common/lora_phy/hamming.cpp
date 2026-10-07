#include "hamming.h"

namespace isac_lora {

static uint8_t parity_bit(uint8_t nibble) {
    // even parity over the 4 data bits
    uint8_t p = 0;
    for (int i = 0; i < 4; ++i) p ^= (nibble >> i) & 0x01;
    return p;
}

std::vector<uint8_t> hamming_encode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        uint8_t hi = (byte >> 4) & 0x0F;
        uint8_t lo = byte & 0x0F;
        out.push_back(static_cast<uint8_t>(hi | (parity_bit(hi) << 4))); // 5-bit codeword
        out.push_back(static_cast<uint8_t>(lo | (parity_bit(lo) << 4)));
    }
    return out;
}

std::vector<uint8_t> hamming_decode(const std::vector<uint8_t>& codewords, bool* ok) {
    std::vector<uint8_t> out;
    out.reserve(codewords.size() / 2);
    bool all_ok = true;
    for (size_t i = 0; i + 1 < codewords.size(); i += 2) {
        uint8_t hi_cw = codewords[i] & 0x1F;
        uint8_t lo_cw = codewords[i + 1] & 0x1F;
        uint8_t hi = hi_cw & 0x0F;
        uint8_t lo = lo_cw & 0x0F;
        uint8_t hi_parity_rx = (hi_cw >> 4) & 0x01;
        uint8_t lo_parity_rx = (lo_cw >> 4) & 0x01;
        if (parity_bit(hi) != hi_parity_rx) all_ok = false;
        if (parity_bit(lo) != lo_parity_rx) all_ok = false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    if (ok) *ok = all_ok;
    return out;
}

} // namespace isac_lora
