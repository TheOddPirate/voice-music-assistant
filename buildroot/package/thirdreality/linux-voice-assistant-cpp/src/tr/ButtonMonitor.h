#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace lva::tr {

// Generic single-key multi-tap monitor for a Linux input device.
//
// A background thread groups key releases of one keycode into a tap count
// using a timing window, then wakes the main loop through an eventfd. The
// registered callback runs on the main loop (via OnMainLoopWake) and decides
// what the tap count means. This keeps the input/debounce logic in one place
// while callers (home button -> HA event, tap key -> sendspin/voice control)
// supply only policy.
class ButtonMonitor {
   public:
    struct Options {
        std::string input_device = "/dev/input/event0";
        int         keycode      = 0;     // e.g. KEY_HOME (102), KEY_SELECT (353)
        int         tap_window_ms = 500;  // multi-tap grouping window
    };

    // Receives the number of taps in a burst (1 = single, 2 = double, ...).
    using TapCallback = std::function<void(int taps)>;

    ButtonMonitor(const Options& opts, TapCallback on_tap);
    ~ButtonMonitor();

    ButtonMonitor(const ButtonMonitor&)            = delete;
    ButtonMonitor& operator=(const ButtonMonitor&) = delete;

    // Returns a readable eventfd to register on the main loop, or -1.
    int  Start();

    // Drain the eventfd and dispatch the pending tap count on the main loop.
    void OnMainLoopWake();

    void Stop();

   private:
    void ThreadLoop();

    Options           opts_;
    TapCallback       on_tap_;

    int               event_fd_ = -1;   // worker -> main
    std::thread       thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<int>  pending_taps_{0};
};

}  // namespace lva::tr
