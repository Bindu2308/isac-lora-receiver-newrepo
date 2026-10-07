// Generates a synthetic "recording" that mimics what Gateway 1's SDR
// capture would actually look like: 2 MHz oversampled IQ, noise floor,
// a burst of silence, then one real fragment (node4/telegram83/frag1
// ground truth) upsampled by repeating each native sample OVERSAMPLE
// times (zero-order hold -- an approximation of a real front-end, good
// enough to test detection/sync/decimation logic end-to-end), with
// additive noise and a random start offset that is NOT symbol-aligned.
#include "../../common/lora_phy/lora_modulator.h"
#include "../../common/protocol/fragment.h"
#include "../../common/config/lora_config.h"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>

using namespace isac_lora;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output.cf32>\n", argv[0]);
        return 1;
    }

    Fragment frag;
    frag.node_id = 0x04;
    frag.telegram_id = 83;
    frag.fragment_id = 1;
    frag.total_fragments = 3;
    std::memcpy(frag.payload.data(), "SAC-LoRa", 8);
    auto wire = frag.serialize();
    std::vector<uint8_t> wire_vec(wire.begin(), wire.end());

    auto native_iq = lora_modulate(wire_vec, LORA_SF);
    std::printf("native (Fs=BW) samples: %zu\n", native_iq.size());

    // Upsample by OVERSAMPLE via zero-order hold.
    std::vector<cf32> oversampled;
    oversampled.reserve(native_iq.size() * static_cast<size_t>(LORA_OVERSAMPLE));
    for (auto& s : native_iq) {
        for (int r = 0; r < LORA_OVERSAMPLE; ++r) oversampled.push_back(s);
    }
    std::printf("oversampled (Fs=2MHz) samples: %zu\n", oversampled.size());

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> offset_dist(0, LORA_OVERSAMPLE * 50); // sub-symbol random offset
    std::normal_distribution<float> noise(0.0f, 0.05f); // additive gaussian noise, modest SNR

    size_t pre_silence = 20000;
    size_t post_silence = 20000;
    int random_offset = offset_dist(rng);

    std::vector<cf32> recording;
    recording.reserve(pre_silence + static_cast<size_t>(random_offset) + oversampled.size() + post_silence);

    auto push_noise = [&](size_t n) {
        for (size_t i = 0; i < n; ++i) recording.emplace_back(noise(rng), noise(rng));
    };

    push_noise(pre_silence);
    push_noise(static_cast<size_t>(random_offset)); // non-symbol-aligned start, forces phase search
    for (auto& s : oversampled) {
        recording.push_back(s + cf32(noise(rng), noise(rng)));
    }
    push_noise(post_silence);

    std::printf("total recording samples: %zu, random_offset=%d (phase=%d)\n",
                recording.size(), random_offset, random_offset % LORA_OVERSAMPLE);

    std::ofstream out(argv[1], std::ios::binary);
    for (auto& s : recording) {
        float re = s.real(), im = s.imag();
        out.write(reinterpret_cast<char*>(&re), sizeof(float));
        out.write(reinterpret_cast<char*>(&im), sizeof(float));
    }
    std::printf("wrote %s\n", argv[1]);
    return 0;
}
