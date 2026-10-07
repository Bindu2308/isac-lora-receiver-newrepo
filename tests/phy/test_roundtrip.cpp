// Acceptance test for Phase 1 (spec section 46): a known transmitted byte
// sequence must decode exactly. We use the REAL, previously-confirmed
// wire bytes from ESP32 node 4, telegram 83, fragment 1:
//   04 00 00 00 53 01 03 53 41 43 2D 4C 6F 52 61
// which is node_id=4, telegram_id=83, fragment_id=1, total_fragments=3,
// payload="SAC-LoRa" -- this is real hardware ground truth, not invented data.
#include "../../common/lora_phy/lora_modulator.h"
#include "../../common/lora_phy/lora_demodulator.h"
#include "../../common/protocol/fragment.h"
#include "../../common/config/lora_config.h"
#include <cstdio>
#include <cstring>

using namespace isac_lora;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("  [FAIL] %s\n", what);
        failures++;
    } else {
        std::printf("  [ OK ] %s\n", what);
    }
}

static void run_case(const char* name, const std::vector<uint8_t>& wire_bytes) {
    std::printf("--- %s (%zu bytes) ---\n", name, wire_bytes.size());

    // TX
    auto iq = lora_modulate(wire_bytes, LORA_SF);
    std::printf("  modulated %zu IQ samples\n", iq.size());

    // RX
    auto result = lora_demodulate(iq, wire_bytes.size(), LORA_SF);
    std::printf("  demod stage: %s, avg confidence: %.2f\n",
                result.stage_name(), result.avg_symbol_confidence);

    check(result.ok, "demodulation reported OK");
    if (result.ok) {
        check(result.payload.size() == wire_bytes.size(), "payload length matches");
        bool bytes_match = (result.payload.size() == wire_bytes.size()) &&
                            std::memcmp(result.payload.data(), wire_bytes.data(), wire_bytes.size()) == 0;
        check(bytes_match, "payload bytes bit-exact match");
        if (!bytes_match) {
            std::printf("  expected:");
            for (auto b : wire_bytes) std::printf(" %02X", b);
            std::printf("\n  got:     ");
            for (auto b : result.payload) std::printf(" %02X", b);
            std::printf("\n");
        }
    }
    std::printf("\n");
}

int main() {
    // --- Real hardware ground truth: node 4, telegram 83, fragment 1 ---
    Fragment frag;
    frag.node_id = 0x04;
    frag.telegram_id = 83;
    frag.fragment_id = 1;
    frag.total_fragments = 3;
    const char* chunk = "SAC-LoRa"; // 8 bytes, matches FRAGMENT_PAYLOAD_SIZE
    std::memcpy(frag.payload.data(), chunk, 8);

    auto wire = frag.serialize();
    std::vector<uint8_t> wire_vec(wire.begin(), wire.end());

    // Sanity-check our serializer against the actual confirmed byte capture.
    const uint8_t ground_truth[15] = {
        0x04, 0x00, 0x00, 0x00, 0x53, 0x01, 0x03,
        0x53, 0x41, 0x43, 0x2D, 0x4C, 0x6F, 0x52, 0x61
    };
    check(std::memcmp(wire_vec.data(), ground_truth, 15) == 0,
          "Fragment::serialize() matches real ESP32 ground-truth bytes");

    run_case("real fragment (node4/telegram83/frag1)", wire_vec);

    // Also test a short generic payload and an all-zero / all-one edge case.
    run_case("simple ascii", {'0','1','0','2','0','3','0','4'});
    run_case("all zero byte", std::vector<uint8_t>(8, 0x00));
    run_case("all 0xFF byte", std::vector<uint8_t>(8, 0xFF));

    // --- Negative test: corrupt one IQ sample's chirp and confirm the
    // chain actually DETECTS it (proves CRC/Hamming aren't rubber-stamping
    // everything as OK -- a vacuous test would pass even if checks were
    // disabled) ---
    {
        std::printf("--- corruption detection (negative test) ---\n");
        auto iq = lora_modulate(wire_vec, LORA_SF);
        // Replace the first payload symbol's chirp with a DIFFERENT valid
        // symbol value (not just a phase flip, which the FFT peak-detector
        // is invariant to) -- this actually changes the decoded bits.
        size_t native_sps = 1u << LORA_SF;
        size_t preamble_samples = static_cast<size_t>(LORA_PREAMBLE_SYMS) * native_sps;
        auto wrong_symbol_wave = modulate_symbol(LORA_SF, 42); // arbitrary wrong symbol
        for (size_t i = 0; i < native_sps; ++i) {
            iq[preamble_samples + i] = wrong_symbol_wave[i];
        }
        auto result = lora_demodulate(iq, wire_vec.size(), LORA_SF);
        std::printf("  demod stage after corruption: %s\n", result.stage_name());
        check(!result.ok, "corrupted symbol is correctly rejected (not silently accepted)");
        std::printf("\n");
    }

    std::printf("=== %s ===\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
