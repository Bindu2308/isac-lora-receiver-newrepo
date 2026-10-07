#pragma once
#include "../../lora_phy/chirp.h"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>

namespace isac_lora {

// A block of IQ samples with a capture timestamp. The SDR callback pushes
// blocks; the demod worker thread pops them. Bounded so a slow worker
// can't cause unbounded memory growth -- if full, the oldest block is
// dropped and a counter incremented (logged by the caller), per spec
// section 43 (IQ queue overflow must be handled, not crash).
struct IqBlock {
    std::vector<cf32> samples;
    uint64_t timestamp_ms = 0;
};

class IqQueue {
public:
    explicit IqQueue(size_t max_blocks) : max_blocks_(max_blocks) {}

    // Called from the SDR callback. Must be fast and non-blocking.
    void push(IqBlock&& block) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_blocks_) {
            queue_.pop_front();
            ++dropped_blocks_;
        }
        queue_.push_back(std::move(block));
        cv_.notify_one();
    }

    // Called from the worker thread. Blocks until a block is available or
    // `stop()` is called.
    bool pop(IqBlock* out) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (queue_.empty()) return false; // woke because of stop()
        *out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    uint64_t dropped_blocks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_blocks_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<IqBlock> queue_;
    size_t max_blocks_;
    bool stopped_ = false;
    uint64_t dropped_blocks_ = 0;
};

} // namespace isac_lora
