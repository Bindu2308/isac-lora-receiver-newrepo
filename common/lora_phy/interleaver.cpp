#include "interleaver.h"

namespace isac_lora {

std::vector<uint16_t> interleave_block(const std::vector<uint8_t>& codewords,
                                        int sf, int cr_plus4) {
    std::vector<uint16_t> out;
    size_t num_blocks = codewords.size() / static_cast<size_t>(sf);
    out.reserve(num_blocks * static_cast<size_t>(cr_plus4));

    for (size_t b = 0; b < num_blocks; ++b) {
        const uint8_t* block = &codewords[b * static_cast<size_t>(sf)];
        std::vector<uint16_t> block_out(static_cast<size_t>(cr_plus4), 0);
        for (int j = 0; j < cr_plus4; ++j) {
            uint16_t sym = 0;
            for (int i = 0; i < sf; ++i) {
                int row = ((i - j) % sf + sf) % sf; // diagonal shift, wrapped
                uint8_t bit = (block[row] >> j) & 0x01;
                sym = static_cast<uint16_t>(sym | (bit << i));
            }
            block_out[static_cast<size_t>(j)] = sym;
        }
        out.insert(out.end(), block_out.begin(), block_out.end());
    }
    return out;
}

std::vector<uint8_t> deinterleave_block(const std::vector<uint16_t>& symbols,
                                         int sf, int cr_plus4) {
    std::vector<uint8_t> out;
    size_t num_blocks = symbols.size() / static_cast<size_t>(cr_plus4);
    out.reserve(num_blocks * static_cast<size_t>(sf));

    for (size_t b = 0; b < num_blocks; ++b) {
        const uint16_t* block = &symbols[b * static_cast<size_t>(cr_plus4)];
        std::vector<uint8_t> block_out(static_cast<size_t>(sf), 0);
        for (int j = 0; j < cr_plus4; ++j) {
            uint16_t sym = block[j];
            for (int i = 0; i < sf; ++i) {
                uint8_t bit = (sym >> i) & 0x01;
                int row = ((i - j) % sf + sf) % sf;
                block_out[static_cast<size_t>(row)] = static_cast<uint8_t>(
                    block_out[static_cast<size_t>(row)] | (bit << j));
            }
        }
        out.insert(out.end(), block_out.begin(), block_out.end());
    }
    return out;
}

} // namespace isac_lora
