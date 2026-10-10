#include "tr/ControlListener.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
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
    ::unlink(kSocketPath);  // clear a stale socket from a previous run

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
    LVA_LOGI(kTag, "listening on %s", kSocketPath);
    return fd_;
}

void ControlListener::OnReadable() {
    if (fd_ < 0) return;
    char buf[64];
    while (true) {
        const ssize_t n = ::recv(fd_, buf, sizeof(buf) - 1, MSG_DONTWAIT);
        if (n <= 0) break;
        buf[n] = '\0';
        if (on_command_) on_command_(buf);
    }
}

}  // namespace lva::tr
