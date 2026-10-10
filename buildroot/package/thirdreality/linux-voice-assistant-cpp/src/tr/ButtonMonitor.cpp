#include "tr/ButtonMonitor.h"

#include <linux/input.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

#include "util/Log.h"

namespace lva::tr {

namespace {

constexpr const char* kTag       = "button";
constexpr int    kEvKey          = 1;    // EV_KEY
constexpr int    kPollTimeoutMs  = 100;

}  // namespace

ButtonMonitor::ButtonMonitor(const Options& opts, TapCallback on_tap)
    : opts_(opts), on_tap_(std::move(on_tap)) {}

ButtonMonitor::~ButtonMonitor() { Stop(); }

int ButtonMonitor::Start() {
    if (event_fd_ >= 0) return event_fd_;
    event_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0) {
        LVA_LOGE(kTag, "eventfd failed: %s", std::strerror(errno));
        return -1;
    }
    stop_requested_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this] { ThreadLoop(); });
    LVA_LOGI(kTag, "started (input=%s, keycode=%d)",
             opts_.input_device.c_str(), opts_.keycode);
    return event_fd_;
}

void ButtonMonitor::Stop() {
    if (!thread_.joinable()) return;
    stop_requested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();
    if (event_fd_ >= 0) {
        ::close(event_fd_);
        event_fd_ = -1;
    }
}

void ButtonMonitor::OnMainLoopWake() {
    std::uint64_t expirations = 0;
    while (::read(event_fd_, &expirations, sizeof(expirations)) > 0) {}

    const int taps = pending_taps_.exchange(0, std::memory_order_acq_rel);
    if (taps <= 0) return;

    if (on_tap_) on_tap_(taps);
}

void ButtonMonitor::ThreadLoop() {
    const int fd = ::open(opts_.input_device.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LVA_LOGE(kTag, "open(%s) failed: %s",
                 opts_.input_device.c_str(), std::strerror(errno));
        return;
    }

    const int window_ms = opts_.tap_window_ms;

    int    tap_count             = 0;
    auto   last_release_time     = std::chrono::steady_clock::time_point{};
    bool   release_pending_flush = false;

    auto flush = [&](int count) {
        pending_taps_.store(count, std::memory_order_release);
        std::uint64_t one = 1;
        ::write(event_fd_, &one, sizeof(one));
    };

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        int poll_timeout = kPollTimeoutMs;
        if (release_pending_flush) {
            const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_release_time).count();
            if (elapsed_ms >= window_ms) {
                flush(tap_count);
                tap_count             = 0;
                release_pending_flush = false;
                continue;
            }
            poll_timeout = static_cast<int>(window_ms - elapsed_ms);
            if (poll_timeout < 1) poll_timeout = 1;
        }

        struct pollfd pfd{};
        pfd.fd      = fd;
        pfd.events  = POLLIN;
        const int pr = ::poll(&pfd, 1, poll_timeout);
        if (pr < 0) {
            if (errno == EINTR) continue;
            LVA_LOGE(kTag, "poll failed: %s", std::strerror(errno));
            break;
        }
        if (pr == 0) continue;  // timeout
        if (!(pfd.revents & POLLIN)) continue;

        struct input_event ev;
        const ssize_t n = ::read(fd, &ev, sizeof(ev));
        if (n < static_cast<ssize_t>(sizeof(ev))) continue;

        // Only key release of the configured keycode counts as a tap.
        if (ev.type != kEvKey) continue;
        if (ev.code != opts_.keycode) continue;
        if (ev.value != 0) continue;  // 0 = release

        const auto now2 = std::chrono::steady_clock::now();
        if (release_pending_flush) {
            const auto since_last =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now2 - last_release_time).count();
            if (since_last < window_ms) {
                ++tap_count;
            } else {
                flush(tap_count);
                tap_count = 1;
            }
        } else {
            tap_count = 1;
        }
        last_release_time     = now2;
        release_pending_flush = true;
    }

    // Flush any pending tap count on shutdown.
    if (tap_count > 0) {
        flush(tap_count);
    }

    ::close(fd);
    LVA_LOGD(kTag, "thread exiting");
}

}  // namespace lva::tr
