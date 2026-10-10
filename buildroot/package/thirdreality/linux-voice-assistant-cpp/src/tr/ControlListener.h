#pragma once

#include <functional>
#include <string>

namespace lva::tr {

// Receiver for tap/home commands sent by tr-buttond over a Unix datagram
// socket. tr-buttond owns /dev/input/event0; lva no longer reads the input
// device. The socket fd is registered on the main loop via AddAuxFd, so the
// callback runs on the main thread with no blocking.
//
// Socket path: /tmp/lva-control.sock
// Commands: "TAP", "HOME_SINGLE", "HOME_DOUBLE".
class ControlListener {
   public:
    using CommandCallback = std::function<void(const std::string& cmd)>;

    explicit ControlListener(CommandCallback on_command);
    ~ControlListener();

    ControlListener(const ControlListener&)            = delete;
    ControlListener& operator=(const ControlListener&) = delete;

    // Binds the datagram socket. Returns a readable fd to register on the
    // main loop, or -1 on failure.
    int Start();

    // Drain all pending datagrams and dispatch recognized commands.
    void OnReadable();

   private:
    CommandCallback on_command_;
    int             fd_ = -1;
};

}  // namespace lva::tr
