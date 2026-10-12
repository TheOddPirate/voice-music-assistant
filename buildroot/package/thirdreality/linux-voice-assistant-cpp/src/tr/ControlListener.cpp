#include "tr/ControlListener.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>

#include "util/Log.h"

namespace lva::tr {

namespace {
constexpr const char* kTag        = "lva-ctl";
constexpr const char* kSocketPath = "/tmp/lva-control.sock";
}  // namespace

ControlListener::ControlListener(CommandCallback on_command)
    : on_command_(std::move(on_command)) {}

ControlListener::~ControlListener() {
    if (fd_ >= 0) {
        ::close(fd_);
        ::unlink(kSocketPath);
    }
}

int ControlListener::Start() {
    fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd_ < 0) {
        LVA_LOGE(kTag, "socket() failed: %s", std::strerror(errno));
        return -1;
    }
    ::unlink(kSocketPath);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path) - 1);
    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        LVA_LOGE(kTag, "bind(%s) failed: %s", kSocketPath,
                 std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return -1;
    }
    if (::chmod(kSocketPath, 0600) != 0) {
        LVA_LOGE(kTag, "chmod(%s) failed: %s", kSocketPath,
                 std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        ::unlink(kSocketPath);
        return -1;
    }
    LVA_LOGI(kTag, "listening on %s", kSocketPath);
    return fd_;
}

void ControlListener::OnReadable() {
    if (fd_ < 0) return;
    char buf[128];
    while (true) {
        struct sockaddr_un peer{};
        socklen_t peer_len = sizeof(peer);
        const ssize_t n = ::recvfrom(
            fd_, buf, sizeof(buf) - 1, MSG_DONTWAIT,
            reinterpret_cast<struct sockaddr*>(&peer), &peer_len);
        if (n <= 0) break;
        buf[n] = '\0';

        const bool handled = on_command_ && on_command_(buf);
        const socklen_t minimum_peer_len =
            static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + 1);
        if (peer_len >= minimum_peer_len) {
            const char* reply = handled ? "OK" : "ERR";
            ::sendto(fd_, reply, std::strlen(reply), MSG_NOSIGNAL,
                     reinterpret_cast<struct sockaddr*>(&peer), peer_len);
        }
    }
}

}  // namespace lva::tr
