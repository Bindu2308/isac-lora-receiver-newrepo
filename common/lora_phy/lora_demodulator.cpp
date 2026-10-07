#include "lora_demodulator.h"
#include "crc.h"
#include "whitening.h"
#include "hamming.h"
#include "interleaver.h"
#include "lora_modulator.h"
#include "../config/lora_config.h"

namespace isac_lora {

namespace {
constexpr int CR_PLUS4 = 5;
}

DemodResult lora_demodulate(const std::vector<cf32>& samples,
                             size_t expected_payload_len,
                             int sf) {
    DemodResult result;
    int native_sps = 1 << sf;

    size_t preamble_samples = static_cast<size_t>(LORA_PREAMBLE_SYMS) * static_cast<size_t>(native_sps);
    size_t num_payload_symbols = lora_payload_symbol_count(expected_payload_len, sf);
    size_t payload_samples = num_payload_symbols * static_cast<size_t>(native_sps);

    if (samples.size() < preamble_samples + payload_samples) {
        result.stage = DecodeStage::PAYLOAD_SYMBOLS_INSUFFICIENT;
        return result;
    }

    // --- preamble check: every preamble symbol should demodulate to 0 ---
    for (int i = 0; i < LORA_PREAMBLE_SYMS; ++i) {
        std::vector<cf32> seg(samples.begin() + static_cast<long>(i) * native_sps,
                               samples.begin() + static_cast<long>(i + 1) * native_sps);
        float conf = 0.0f;
        uint16_t sym = demodulate_symbol(sf, seg, &conf);
        if (sym != 0) {
            result.stage = DecodeStage::PREAMBLE_NOT_FOUND;
            return result;
        }
    }

    // --- demodulate payload symbols ---
    std::vector<uint16_t> symbols;
    symbols.reserve(num_payload_symbols);
    float conf_sum = 0.0f;
    for (size_t i = 0; i < num_payload_symbols; ++i) {
        size_t start = preamble_samples + i * static_cast<size_t>(native_sps);
        std::vector<cf32> seg(samples.begin() + static_cast<long>(start),
                               samples.begin() + static_cast<long>(start + static_cast<size_t>(native_sps)));
        float conf = 0.0f;
        uint16_t raw_sym = demodulate_symbol(sf, seg, &conf);
        conf_sum += conf;
        symbols.push_back(gray_decode(raw_sym));
    }
    result.avg_symbol_confidence = conf_sum / static_cast<float>(num_payload_symbols);

    // --- deinterleave -> codewords ---
    std::vector<uint8_t> codewords = deinterleave_block(symbols, sf, CR_PLUS4);

    // --- Hamming decode -> whitened data+crc bytes ---
    bool hamming_ok = false;
    std::vector<uint8_t> data = hamming_decode(codewords, &hamming_ok);
    if (!hamming_ok) {
        result.stage = DecodeStage::HAMMING_PARITY_FAIL;
        return result;
    }

    // Trim any zero-padding codewords added by the modulator down to the
    // real data length (payload + 2 CRC bytes).
    size_t real_data_len = expected_payload_len + 2;
    if (data.size() > real_data_len) data.resize(real_data_len);

    // --- dewhiten ---
    dewhiten(data.data(), data.size());

    // --- CRC check ---
    if (data.size() < 2) {
        result.stage = DecodeStage::CRC_FAIL;
        return result;
    }
    std::vector<uint8_t> payload(data.begin(), data.end() - 2);
    uint16_t rx_crc = static_cast<uint16_t>((data[data.size() - 2] << 8) | data[data.size() - 1]);
    uint16_t calc_crc = crc16_ccitt(payload.data(), payload.size());
    if (rx_crc != calc_crc) {
        result.stage = DecodeStage::CRC_FAIL;
        return result;
    }

    result.stage = DecodeStage::OK;
    result.ok = true;
    result.payload = payload;
    return result;
}

} // namespace isac_lora
