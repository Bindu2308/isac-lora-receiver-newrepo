#include "gateway_runner.h"
#include "../config/lora_config.h"
#include "../protocol/observation.h"
#include "../protocol/fragment.h"
#include "buffer/iq_queue.h"
#include "detection/signal_detector.h"
#include "synchronization/synchronizer.h"
#include "network/gateway_client.h"
#include "sdr/sdrplay_device.h"

#include <cstdio>
#include <chrono>
#include <memory>
#include <algorithm>

namespace isac_lora {

namespace {

uint64_t now_ms() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void process_candidate(const Candidate& cand,
                        const std::vector<cf32>& iq,
                        uint8_t gateway_id,
                        GatewayClient* client) {
    std::vector<cf32> region(iq.begin() + static_cast<long>(cand.start_sample),
                              iq.begin() + static_cast<long>(std::min(cand.end_sample, iq.size())));

    SyncResult sync = synchronize(region, LORA_SF, LORA_OVERSAMPLE, FRAGMENT_WIRE_SIZE);
    if (!sync.success) {
        std::printf("[gw%u] candidate at %zu: no sync alignment found (conf=%.1f dB)\n",
                    gateway_id, cand.start_sample, cand.confidence);
        return;
    }

    GatewayObservation obs;
    obs.gateway_id = gateway_id;
    obs.timestamp_ms = now_ms();
    obs.received_power_db = cand.power_db;
    obs.noise_power_db = cand.noise_db;
    obs.sync_confidence = cand.confidence;
    obs.demod_confidence = sync.demod.avg_symbol_confidence;
    obs.crc_ok = sync.demod.ok ? 1 : 0;

    if (sync.demod.ok && sync.demod.payload.size() >= 7) {
        const auto& p = sync.demod.payload;
        obs.node_id = p[0];
        obs.telegram_id = (static_cast<uint32_t>(p[1]) << 24) | (static_cast<uint32_t>(p[2]) << 16) |
                           (static_cast<uint32_t>(p[3]) << 8) | static_cast<uint32_t>(p[4]);
        obs.fragment_id = p[5];
        obs.total_fragments = p[6];
        for (size_t i = 0; i < OBS_PAYLOAD_SIZE && (7 + i) < p.size(); ++i) {
            obs.payload[i] = p[7 + i];
        }
        std::printf("[gw%u] DECODED node=%u telegram=%u frag=%u/%u stage=%s conf=%.2f\n",
                    gateway_id, obs.node_id, obs.telegram_id, obs.fragment_id, obs.total_fragments,
                    sync.demod.stage_name(), obs.demod_confidence);
    } else {
        std::printf("[gw%u] sync found but decode failed at stage=%s\n",
                    gateway_id, sync.demod.stage_name());
    }

    if (!client->send(obs)) {
        std::printf("[gw%u] WARNING: failed to send observation to RPi4\n", gateway_id);
    }
}

} // namespace

int run_gateway(const GatewayConfig& config) {
    std::printf("ISAC-LoRa Gateway %u starting (mode=%s)\n",
                config.gateway_id, config.live_mode ? "live" : "replay");
    std::printf("  freq=%.1f MHz  BW=%.0f kHz  SF=%d  oversample=%d  gain=%.1f dB  rpi4=%s:%u\n",
                LORA_FREQ_HZ / 1e6, LORA_BANDWIDTH_HZ / 1e3, LORA_SF, LORA_OVERSAMPLE,
                config.gain_db, config.rpi4_host.c_str(), config.rpi4_port);

    IqQueue queue(64);

    std::unique_ptr<IqSource> source;
    if (config.live_mode) {
        source = std::make_unique<SdrplaySource>(LORA_FREQ_HZ, LORA_SAMPLE_RATE_HZ, config.gain_db);
    } else {
        source = std::make_unique<FileReplaySource>(config.replay_file, 8192, /*throttle=*/false);
    }

    if (!source->start(&queue)) {
        std::fprintf(stderr, "Gateway %u: failed to start IQ source\n", config.gateway_id);
        return 1;
    }

    GatewayClient client(config.rpi4_host, config.rpi4_port);
    SignalDetector detector;

    std::vector<cf32> analysis_buffer;
    const size_t analysis_chunk_target = 200000;

    IqBlock block;
    while (queue.pop(&block)) {
        analysis_buffer.insert(analysis_buffer.end(), block.samples.begin(), block.samples.end());
        if (analysis_buffer.size() < analysis_chunk_target) continue;

        auto candidates = detector.detect(analysis_buffer);
        std::printf("[gw%u] analyzed %zu samples, %zu candidate(s)\n",
                    config.gateway_id, analysis_buffer.size(), candidates.size());
        for (const auto& c : candidates) {
            process_candidate(c, analysis_buffer, config.gateway_id, &client);
        }
        analysis_buffer.clear();
    }
    if (!analysis_buffer.empty()) {
        auto candidates = detector.detect(analysis_buffer);
        std::printf("[gw%u] final flush: analyzed %zu samples, %zu candidate(s)\n",
                    config.gateway_id, analysis_buffer.size(), candidates.size());
        for (const auto& c : candidates) {
            process_candidate(c, analysis_buffer, config.gateway_id, &client);
        }
    }

    source->stop();
    std::printf("[gw%u] dropped IQ blocks: %llu\n",
                config.gateway_id, static_cast<unsigned long long>(queue.dropped_blocks()));
    return 0;
}

} // namespace isac_lora
