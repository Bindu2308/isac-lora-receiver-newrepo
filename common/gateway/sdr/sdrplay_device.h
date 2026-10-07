#pragma once
#include "../../lora_phy/chirp.h"
#include "../buffer/iq_queue.h"
#include <cstdint>
#include <atomic>
#include <thread>
#include <string>

namespace isac_lora {

// Common interface so main.cpp doesn't care whether IQ comes from real
// SDRplay hardware or a replayed/synthetic file (spec section 36, Mode 1
// vs Mode 2/3). Implementations push IqBlocks into the shared queue from
// their own capture thread and must return quickly from start()/stop().
class IqSource {
public:
    virtual ~IqSource() = default;
    virtual bool start(IqQueue* queue) = 0;
    virtual void stop() = 0;
};

// ============================================================
// HARDWARE-ONLY: requires SoapySDR + SoapySDRPlay3 module installed on
// the Raspberry Pi, and a physically connected SDRplay RSP1B. This class
// CANNOT be built or tested without that hardware/driver stack -- do not
// expect it to compile in a plain sandbox environment. See gateway1/README
// (build instructions) for the apt/pip packages needed on the Pi.
// ============================================================
class SdrplaySource : public IqSource {
public:
    SdrplaySource(double freq_hz, double sample_rate_hz, double gain_db);
    ~SdrplaySource() override;

    bool start(IqQueue* queue) override;
    void stop() override;

private:
    double freq_hz_;
    double sample_rate_hz_;
    double gain_db_;
    void* device_ = nullptr; // opaque SoapySDR::Device* (kept opaque to avoid
                              // forcing SoapySDR headers on non-hardware builds)
    std::atomic<bool> running_{false};
    std::thread capture_thread_;
};

// ============================================================
// Offline replay source: reads raw interleaved float32 IQ samples from a
// .cf32 file (format: I0,Q0,I1,Q1,... as little-endian float32) and feeds
// them into the queue in fixed-size blocks, at a throttled rate that
// mimics real-time capture. No hardware required -- this is what we use
// to validate the pipeline right now, and it's also useful later for
// replaying real captured RF for regression testing (spec section 36).
// ============================================================
class FileReplaySource : public IqSource {
public:
    explicit FileReplaySource(std::string filepath, size_t block_size = 8192, bool throttle = false);
    ~FileReplaySource() override;

    bool start(IqQueue* queue) override;
    void stop() override;

private:
    std::string filepath_;
    size_t block_size_;
    bool throttle_;
    std::atomic<bool> running_{false};
    std::thread replay_thread_;
};

} // namespace isac_lora
