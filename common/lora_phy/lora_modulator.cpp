#include "lora_modulator.h"
#include "crc.h"
#include "whitening.h"
#include "hamming.h"
#include "interleaver.h"
#include "../config/lora_config.h"

namespace isac_lora {

namespace {
constexpr int CR_PLUS4 = 5; // coding rate 4/5 -> codeword width 5 bits

size_t padded_codeword_count(size_t data_bytes, int sf) {
    size_t num_codewords = data_bytes * 2; // 2 nibbles per byte
    size_t remainder = num_codewords % static_cast<size_t>(sf);
    if (remainder != 0) {
        num_codewords += (static_cast<size_t>(sf) - remainder);
    }
    return num_codewords;
}
} // namespace

size_t lora_payload_symbol_count(size_t payload_len, int sf) {
    size_t data_bytes = payload_len + 2; // +2 for CRC16
    size_t padded_cw = padded_codeword_count(data_bytes, sf);
    return (padded_cw / static_cast<size_t>(sf)) * static_cast<size_t>(CR_PLUS4);
}

std::vector<cf32> lora_modulate(const std::vector<uint8_t>& payload, int sf) {
    // 1. Append CRC16 over the raw payload.
    uint16_t crc = crc16_ccitt(payload.data(), payload.size());
    std::vector<uint8_t> data = payload;
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(crc & 0xFF));

    // 2. Whiten.
    whiten(data.data(), data.size());

    // 3. Hamming(4/5) encode -> 5-bit codewords, 2 per byte.
    std::vector<uint8_t> codewords = hamming_encode(data);

    // 4. Zero-pad codewords to a multiple of sf (both sides compute this
    //    identically from payload length, so no extra signaling needed).
    size_t padded_len = padded_codeword_count(data.size(), sf);
    codewords.resize(padded_len, 0);

    // 5. Diagonal interleave -> symbol values (low `sf` bits each).
    std::vector<uint16_t> symbols = interleave_block(codewords, sf, CR_PLUS4);

    // 6. Gray encode each symbol, 7. chirp-modulate, concatenate.
    std::vector<cf32> waveform;
    waveform.reserve((static_cast<size_t>(LORA_PREAMBLE_SYMS) + symbols.size()) *
                      static_cast<size_t>(1 << sf));

    // Preamble: LORA_PREAMBLE_SYMS repetitions of the base upchirp (symbol 0).
    for (int i = 0; i < LORA_PREAMBLE_SYMS; ++i) {
        auto sym_wave = modulate_symbol(sf, 0);
        waveform.insert(waveform.end(), sym_wave.begin(), sym_wave.end());
    }

    for (uint16_t sym : symbols) {
        uint16_t gray_sym = gray_encode(sym);
        auto sym_wave = modulate_symbol(sf, gray_sym);
        waveform.insert(waveform.end(), sym_wave.begin(), sym_wave.end());
    }

    return waveform;
}

} // namespace isac_lora
