#include "sdrplay_device.h"
#include <cstdio>
#include <fstream>
#include <chrono>

// The SdrplaySource implementation is guarded behind
// ISAC_LORA_HAVE_SOAPYSDR so this file (and the whole gateway1 target)
// compiles cleanly on a machine without SoapySDR installed -- e.g. right
// here in the offline dev sandbox. On the actual Gateway 1 Raspberry Pi,
// build with -DISAC_LORA_HAVE_SOAPYSDR=1 (see gateway1/CMakeLists.txt)
// after installing:
//   sudo apt install soapysdr-tools libsoapysdr-dev
//   + SoapySDRPlay3 module (from SDRplay's own repo/instructions)
#ifdef ISAC_LORA_HAVE_SOAPYSDR
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#endif

namespace isac_lora {

// ================= SdrplaySource =================

SdrplaySource::SdrplaySource(double freq_hz, double sample_rate_hz, double gain_db)
    : freq_hz_(freq_hz), sample_rate_hz_(sample_rate_hz), gain_db_(gain_db) {}

SdrplaySource::~SdrplaySource() { stop(); }

bool SdrplaySource::start(IqQueue* queue) {
#ifndef ISAC_LORA_HAVE_SOAPYSDR
    (void)queue;
    std::fprintf(stderr,
        "SdrplaySource::start() called but this binary was built WITHOUT "
        "ISAC_LORA_HAVE_SOAPYSDR. Rebuild on the Pi with SoapySDR installed, "
        "or use FileReplaySource for offline testing.\n");
    return false;
#else
    try {
        SoapySDR::Kwargs args;
        args["driver"] = "sdrplay";
        auto* dev = SoapySDR::Device::make(args);
        device_ = dev;

        dev->setSampleRate(SOAPY_SDR_RX, 0, sample_rate_hz_);
        dev->setFrequency(SOAPY_SDR_RX, 0, freq_hz_);
        dev->setGain(SOAPY_SDR_RX, 0, gain_db_);

        auto stream = dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
        dev->activateStream(stream);

        running_ = true;
        capture_thread_ = std::thread([this, dev, stream, queue] {
            const size_t block_len = 8192;
            std::vector<cf32> buf(block_len);
            void* buffs[1] = { buf.data() };
            while (running_) {
                int flags = 0;
                long long time_ns = 0;
                int ret = dev->readStream(stream, buffs, block_len, flags, time_ns, 200000);
                if (ret > 0) {
                    IqBlock block;
                    block.samples.assign(buf.begin(), buf.begin() + ret);
                    block.timestamp_ms = static_cast<uint64_t>(time_ns / 1000000);
                    queue->push(std::move(block));
                }
            }
            dev->deactivateStream(stream);
            dev->closeStream(stream);
        });
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SdrplaySource: SoapySDR error: %s\n", e.what());
        return false;
    }
#endif
}

void SdrplaySource::stop() {
    running_ = false;
    if (capture_thread_.joinable()) capture_thread_.join();
#ifdef ISAC_LORA_HAVE_SOAPYSDR
    if (device_) {
        SoapySDR::Device::unmake(static_cast<SoapySDR::Device*>(device_));
        device_ = nullptr;
    }
#endif
}

// ================= FileReplaySource =================

FileReplaySource::FileReplaySource(std::string filepath, size_t block_size, bool throttle)
    : filepath_(std::move(filepath)), block_size_(block_size), throttle_(throttle) {}

FileReplaySource::~FileReplaySource() { stop(); }

bool FileReplaySource::start(IqQueue* queue) {
    std::ifstream file(filepath_, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "FileReplaySource: cannot open '%s'\n", filepath_.c_str());
        return false;
    }
    running_ = true;
    replay_thread_ = std::thread([this, queue, filepath = filepath_] {
        std::ifstream f(filepath, std::ios::binary);
        std::vector<float> raw(block_size_ * 2);
        uint64_t ts = 0;
        while (running_ && f.read(reinterpret_cast<char*>(raw.data()),
                                   static_cast<std::streamsize>(raw.size() * sizeof(float)))) {
            IqBlock block;
            block.samples.resize(block_size_);
            for (size_t i = 0; i < block_size_; ++i) {
                block.samples[i] = cf32(raw[2 * i], raw[2 * i + 1]);
            }
            block.timestamp_ms = ts;
            ts += 1;
            queue->push(std::move(block));
            if (throttle_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        running_ = false;
        queue->stop(); // signal main loop that no more IQ is coming (replay finished)
    });
    return true;
}

void FileReplaySource::stop() {
    running_ = false;
    if (replay_thread_.joinable()) replay_thread_.join();
}

} // namespace isac_lora
