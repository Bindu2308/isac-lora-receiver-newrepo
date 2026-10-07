#pragma once
#include "../../protocol/observation.h"
#include <string>
#include <cstdint>

namespace isac_lora {

// Simple TCP client, length-prefixed framing: [2-byte big-endian length][observation bytes].
// One observation per send() call. Reconnects lazily on the next send()
// if the connection was lost -- this is deliberately simple for Phase
// 3/4 bring-up (spec section 43: TCP disconnect must not crash the
// gateway, just log and retry).
class GatewayClient {
public:
    GatewayClient(std::string host, uint16_t port);
    ~GatewayClient();

    // Returns true if the observation was sent successfully.
    bool send(const GatewayObservation& obs);

    bool is_connected() const { return sock_fd_ >= 0; }

private:
    bool connect_if_needed();
    void disconnect();

    std::string host_;
    uint16_t port_;
    int sock_fd_ = -1;
};

} // namespace isac_lora
