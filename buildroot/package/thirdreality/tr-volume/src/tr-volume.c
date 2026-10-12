#define _GNU_SOURCE

#include <pulse/pulseaudio.h>
#include <pulse/timeval.h>
#include <pulse/volume.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

struct volume_request {
    pa_mainloop *mainloop;
    pa_context *context;
    pa_volume_t volume;
    bool lookup_started;
    bool set_started;
    bool finished;
    bool success;
};

static bool parse_percent(const char *text, int *percent) {
    char *end = NULL;
    long value;

    if (text == NULL || *text == '\0') return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < 0 || value > 100 || value > INT_MAX) {
        return false;
    }
    *percent = (int)value;
    return true;
}

static int claim_latest_request(int percent) {
    const char *pending_path = "/tmp/tr-volume.pending";
    const char *apply_lock_path = "/tmp/tr-volume.apply.lock";
    char value[16];
    char current[16] = {0};
    int pending_fd;
    int apply_fd;
    int length;
    ssize_t count;

    pending_fd = open(pending_path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (pending_fd < 0 || flock(pending_fd, LOCK_EX) != 0) {
        if (pending_fd >= 0) close(pending_fd);
        return -1;
    }
    length = snprintf(value, sizeof(value), "%d\n", percent);
    if (ftruncate(pending_fd, 0) != 0 ||
        lseek(pending_fd, 0, SEEK_SET) < 0 ||
        write(pending_fd, value, (size_t)length) != length) {
        flock(pending_fd, LOCK_UN);
        close(pending_fd);
        return -1;
    }
    flock(pending_fd, LOCK_UN);
    close(pending_fd);

    apply_fd = open(apply_lock_path, O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (apply_fd < 0 || flock(apply_fd, LOCK_EX) != 0) {
        if (apply_fd >= 0) close(apply_fd);
        return -1;
    }

    pending_fd = open(pending_path, O_RDONLY | O_CLOEXEC);
    if (pending_fd < 0 || flock(pending_fd, LOCK_SH) != 0) {
        if (pending_fd >= 0) close(pending_fd);
        close(apply_fd);
        return -1;
    }
    count = read(pending_fd, current, sizeof(current) - 1);
    flock(pending_fd, LOCK_UN);
    close(pending_fd);
    if (count <= 0 || atoi(current) != percent) {
        flock(apply_fd, LOCK_UN);
        close(apply_fd);
        return -2;
    }
    return apply_fd;
}

static void finish(struct volume_request *request, bool success) {
    if (request->finished) return;
    request->finished = true;
    request->success = success;
    pa_mainloop_quit(request->mainloop, success ? 0 : 1);
}

static void on_timeout(pa_mainloop_api *api, pa_time_event *event,
                       const struct timeval *time, void *userdata) {
    (void)api;
    (void)event;
    (void)time;
    finish(userdata, false);
}

static void on_volume_set(pa_context *context, int success, void *userdata) {
    (void)context;
    finish(userdata, success != 0);
}

static void on_sink_info(pa_context *context, const pa_sink_info *info,
                         int eol, void *userdata) {
    struct volume_request *request = userdata;
    pa_cvolume volume;
    pa_operation *operation;

    if (eol < 0) {
        finish(request, false);
        return;
    }
    if (eol > 0) {
        if (!request->set_started) finish(request, false);
        return;
    }
    if (info == NULL || request->set_started) return;

    request->set_started = true;
    pa_cvolume_set(&volume, info->channel_map.channels, request->volume);
    operation = pa_context_set_sink_volume_by_name(
        context, info->name, &volume, on_volume_set, request);
    if (operation == NULL) {
        finish(request, false);
        return;
    }
    pa_operation_unref(operation);
}

static void on_server_info(pa_context *context, const pa_server_info *info,
                           void *userdata) {
    struct volume_request *request = userdata;
    pa_operation *operation;

    if (info == NULL || info->default_sink_name == NULL) {
        finish(request, false);
        return;
    }
    operation = pa_context_get_sink_info_by_name(
        context, info->default_sink_name, on_sink_info, request);
    if (operation == NULL) {
        finish(request, false);
        return;
    }
    pa_operation_unref(operation);
}

static void on_context_state(pa_context *context, void *userdata) {
    struct volume_request *request = userdata;
    pa_operation *operation;

    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_READY:
            if (request->lookup_started) return;
            request->lookup_started = true;
            operation = pa_context_get_server_info(
                context, on_server_info, request);
            if (operation == NULL) {
                finish(request, false);
                return;
            }
            pa_operation_unref(operation);
            return;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            finish(request, false);
            return;
        default:
            return;
    }
}

int main(int argc, char *argv[]) {
    struct volume_request request = {0};
    struct timeval deadline;
    pa_mainloop_api *api;
    double logical;
    double linear_gain;
    int percent = 0;
    int result = 1;
    int apply_lock_fd;

    if (argc != 3 || strcmp(argv[1], "set") != 0) {
        fprintf(stderr, "Usage: tr-volume set <0-100>\n");
        return 2;
    }
    if (!parse_percent(argv[2], &percent)) {
        fprintf(stderr, "[tr-volume] Error: invalid volume '%s'\n", argv[2]);
        return 2;
    }

    apply_lock_fd = claim_latest_request(percent);
    if (apply_lock_fd == -2) {
        return 0;
    }
    if (apply_lock_fd < 0) {
        fprintf(stderr, "[tr-volume] Error: failed to serialize request\n");
        return 1;
    }

    logical = (double)percent / 100.0;
    linear_gain = logical * logical;
    request.volume = pa_sw_volume_from_linear(linear_gain);
    request.mainloop = pa_mainloop_new();
    if (request.mainloop == NULL) {
        fprintf(stderr, "[tr-volume] Error: pa_mainloop_new failed\n");
        return 1;
    }

    request.context = pa_context_new(
        pa_mainloop_get_api(request.mainloop), "tr-volume");
    if (request.context == NULL) {
        fprintf(stderr, "[tr-volume] Error: pa_context_new failed\n");
        pa_mainloop_free(request.mainloop);
        return 1;
    }

    fprintf(stderr,
            "[tr-volume] Setting %d%% (linear_gain=%.4f, pa_volume=%u)\n",
            percent, linear_gain, (unsigned)request.volume);
    pa_context_set_state_callback(
        request.context, on_context_state, &request);

    pa_gettimeofday(&deadline);
    pa_timeval_add(&deadline, 3 * PA_USEC_PER_SEC);
    api = pa_mainloop_get_api(request.mainloop);
    if (api->time_new(api, &deadline, on_timeout, &request) == NULL) {
        fprintf(stderr, "[tr-volume] Error: timeout setup failed\n");
        pa_context_unref(request.context);
        pa_mainloop_free(request.mainloop);
        return 1;
    }
    if (pa_context_connect(request.context, NULL, PA_CONTEXT_NOFLAGS,
                           NULL) < 0) {
        fprintf(stderr, "[tr-volume] Error: connection failed: %s\n",
                pa_strerror(pa_context_errno(request.context)));
        pa_context_unref(request.context);
        pa_mainloop_free(request.mainloop);
        return 1;
    }

    if (pa_mainloop_run(request.mainloop, &result) < 0) {
        request.success = false;
    }
    pa_context_disconnect(request.context);
    pa_context_unref(request.context);
    pa_mainloop_free(request.mainloop);
    flock(apply_lock_fd, LOCK_UN);
    close(apply_lock_fd);

    if (!request.success) {
        fprintf(stderr, "[tr-volume] Error: volume update failed\n");
        return 1;
    }
    fprintf(stderr, "[tr-volume] Volume set to %d%%\n", percent);
    return 0;
}
