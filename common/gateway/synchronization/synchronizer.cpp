#include "synchronizer.h"
#include "../../lora_phy/lora_modulator.h"
#include "../../config/lora_config.h"

namespace isac_lora {

SyncResult synchronize(const std::vector<cf32>& raw_oversampled_candidate,
                        int sf,
                        int oversample,
                        size_t expected_payload_len) {
    SyncResult result;

    int native_sps = 1 << sf;
    size_t preamble_native_samples = static_cast<size_t>(LORA_PREAMBLE_SYMS) * static_cast<size_t>(native_sps);
    size_t payload_native_samples = lora_payload_symbol_count(expected_payload_len, sf) * static_cast<size_t>(native_sps);
    size_t needed_native_samples = preamble_native_samples + payload_native_samples;
    size_t needed_raw_samples = needed_native_samples * static_cast<size_t>(oversample);

    if (raw_oversampled_candidate.size() < needed_raw_samples) {
        return result; // not enough samples in this candidate to even try
    }

    for (int phase = 0; phase < oversample; ++phase) {
        // Decimate this phase across the whole candidate.
        std::vector<cf32> decimated;
        decimated.reserve(raw_oversampled_candidate.size() / static_cast<size_t>(oversample));
        for (size_t i = static_cast<size_t>(phase); i < raw_oversampled_candidate.size(); i += static_cast<size_t>(oversample)) {
            decimated.push_back(raw_oversampled_candidate[i]);
        }
        if (decimated.size() < needed_native_samples) continue;

        // Slide a native-rate window across the decimated stream, one
        // native sample at a time. Cheap pre-check first: demodulate just
        // the first candidate symbol (a single FFT) and only run the full
        // decode chain (which itself does ~38 FFTs) when that symbol looks
        // like a preamble symbol (0). This keeps the brute-force search
        // fast enough to actually run, without changing correctness --
        // the full lora_demodulate() call still makes the final decision.
        size_t max_start = decimated.size() - needed_native_samples;
        for (size_t start = 0; start <= max_start; ++start) {
            std::vector<cf32> first_symbol(decimated.begin() + static_cast<long>(start),
                                            decimated.begin() + static_cast<long>(start + static_cast<size_t>(native_sps)));
            uint16_t quick_check = demodulate_symbol(sf, first_symbol);
            if (quick_check != 0) continue; // cheap reject, skip the expensive full decode

            std::vector<cf32> window(decimated.begin() + static_cast<long>(start),
                                      decimated.begin() + static_cast<long>(start + needed_native_samples));
            DemodResult demod = lora_demodulate(window, expected_payload_len, sf);
            if (demod.stage != DecodeStage::PREAMBLE_NOT_FOUND) {
                // We found correct preamble alignment (stage progressed
                // past preamble search), regardless of whether CRC/Hamming
                // ultimately passed -- this IS the packet, decoded or not.
                result.success = true;
                result.raw_packet_start = start * static_cast<size_t>(oversample) + static_cast<size_t>(phase);
                result.decimation_phase = phase;
                result.demod = demod;
                return result;
            }
        }
    }

    return result; // no alignment found in any phase
}

} // namespace isac_lora
