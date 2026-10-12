#pragma once

#include <functional>
#include <string>

namespace lva::tr {

// Receives local control requests over /tmp/lva-control.sock. The socket is
// registered with the main loop, so command callbacks run on the main thread.
// A sender that binds a return address receives "OK" or "ERR" after dispatch.
class ControlListener {
   public:
    using CommandCallback = std::function<bool(const std::string& cmd)>;

    explicit ControlListener(CommandCallback on_command);
    ~ControlListener();

    ControlListener(const ControlListener&)            = delete;
    ControlListener& operator=(const ControlListener&) = delete;

    int Start();
    void OnReadable();

   private:
    CommandCallback on_command_;
    int             fd_ = -1;
};

}  // namespace lva::tr
