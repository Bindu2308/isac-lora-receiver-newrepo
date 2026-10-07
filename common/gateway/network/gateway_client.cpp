#include "gateway_client.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace isac_lora {

GatewayClient::GatewayClient(std::string host, uint16_t port)
    : host_(std::move(host)), port_(port) {}

GatewayClient::~GatewayClient() { disconnect(); }

void GatewayClient::disconnect() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool GatewayClient::connect_if_needed() {
    if (sock_fd_ >= 0) return true;

    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        std::perror("GatewayClient: socket() failed");
        return false;
    }

    int one = 1;
    setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        std::fprintf(stderr, "GatewayClient: invalid host IP '%s'\n", host_.c_str());
        disconnect();
        return false;
    }

    // Non-blocking connect with an explicit timeout (spec section 43: a
    // TCP disconnect/unreachable-peer must not hang the gateway forever --
    // the default blocking connect() can take minutes on some networks).
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    int ret = ::connect(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        std::perror("GatewayClient: connect() failed");
        disconnect();
        return false;
    }

    if (ret < 0) { // EINPROGRESS: wait up to 2s for it to complete
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock_fd_, &wfds);
        timeval tv{2, 0};
        int sel = select(sock_fd_ + 1, nullptr, &wfds, nullptr, &tv);
        if (sel <= 0) {
            std::fprintf(stderr, "GatewayClient: connect to %s:%u timed out\n", host_.c_str(), port_);
            disconnect();
            return false;
        }
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            std::fprintf(stderr, "GatewayClient: connect to %s:%u failed: %s\n",
                        host_.c_str(), port_, std::strerror(so_error));
            disconnect();
            return false;
        }
    }

    fcntl(sock_fd_, F_SETFL, flags); // restore blocking mode for send()
    std::printf("GatewayClient: connected to %s:%u\n", host_.c_str(), port_);
    return true;
}

bool GatewayClient::send(const GatewayObservation& obs) {
    if (!connect_if_needed()) return false;

    auto payload = obs.serialize();
    uint16_t len = static_cast<uint16_t>(payload.size());
    uint8_t header[2] = { static_cast<uint8_t>((len >> 8) & 0xFF), static_cast<uint8_t>(len & 0xFF) };

    ssize_t sent_hdr = ::send(sock_fd_, header, sizeof(header), 0);
    ssize_t sent_body = ::send(sock_fd_, payload.data(), payload.size(), 0);

    if (sent_hdr != static_cast<ssize_t>(sizeof(header)) ||
        sent_body != static_cast<ssize_t>(payload.size())) {
        std::perror("GatewayClient: send() failed, will reconnect next call");
        disconnect();
        return false;
    }
    return true;
}

} // namespace isac_lora
