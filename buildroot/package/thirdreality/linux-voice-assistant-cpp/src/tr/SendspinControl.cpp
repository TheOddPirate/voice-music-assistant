#include "tr/SendspinControl.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag        = "sendspin-ctl";
constexpr const char* kSocketPath = "/tmp/sendspin-control.sock";

void SendCommand(const char* cmd) {
    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LVA_LOGW(kTag, "socket() failed: %s", std::strerror(errno));
        return;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path) - 1);

    const ssize_t n = ::sendto(fd, cmd, std::strlen(cmd), MSG_NOSIGNAL,
                               reinterpret_cast<struct sockaddr*>(&addr),
                               sizeof(addr));
    if (n < 0) {
        // Expected when sendspin-client is not running (no bound socket).
        LVA_LOGD(kTag, "sendto('%s') failed: %s", cmd, std::strerror(errno));
    } else {
        LVA_LOGD(kTag, "sent '%s'", cmd);
    }

    ::close(fd);
}

}  // namespace

void SendspinMusicToggle() { SendCommand("MUSIC_TOGGLE"); }
void SendspinPause()       { SendCommand("PAUSE"); }
void SendspinLedToggle()   { SendCommand("LED_TOGGLE"); }
void SendspinDuck()        { SendCommand("DUCK"); }
void SendspinUnduck()      { SendCommand("UNDUCK"); }

}  // namespace lva::tr
