#include <stdint.h>

enum { ACT_NONE, ACT_CLICK, ACT_LONG, ACT_DOUBLE };
enum { KEY_CODE = 735, LONG_MS = 1000, DOUBLE_MS = 350 };

struct gesture {
    int down;
    int long_fired;
    int pending;
    int64_t down_at;
    int64_t first_up;
};

static int gesture_due(struct gesture *g, int64_t now)
{
    if (g->pending && now - g->first_up >= DOUBLE_MS) {
        g->pending = 0;
        return ACT_CLICK;
    }
    if (g->down && !g->long_fired && now - g->down_at >= LONG_MS) {
        g->long_fired = 1;
        return ACT_LONG;
    }
    return ACT_NONE;
}

static int gesture_event(struct gesture *g, int value, int64_t now, int double_enabled)
{
    if (value == 1) {
        if (!g->down) {
            g->down = 1;
            g->long_fired = 0;
            g->down_at = now;
        }
        return ACT_NONE;
    }
    if (value != 0 || !g->down)
        return ACT_NONE;
    g->down = 0;
    if (!g->long_fired && now - g->down_at >= LONG_MS) {
        g->long_fired = 1;
        return ACT_LONG;
    }
    if (g->long_fired)
        return ACT_NONE;
    if (!double_enabled) {
        g->pending = 0;
        return ACT_CLICK;
    }
    if (g->pending && now - g->first_up >= 0 && now - g->first_up <= DOUBLE_MS) {
        g->pending = 0;
        return ACT_DOUBLE;
    }
    if (g->pending) {
        g->first_up = now;
        return ACT_CLICK;
    }
    g->pending = 1;
    g->first_up = now;
    return ACT_NONE;
}

static int gesture_timeout(const struct gesture *g, int64_t now)
{
    int timeout = -1;
    int64_t left;
    if (g->pending) {
        left = DOUBLE_MS - (now - g->first_up);
        timeout = left > 0 ? (int)left : 0;
    }
    if (g->down && !g->long_fired) {
        left = LONG_MS - (now - g->down_at);
        if (left < 0)
            left = 0;
        if (timeout < 0 || left < timeout)
            timeout = (int)left;
    }
    return timeout;
}

#ifdef SELFTEST

static int test_gestures(void)
{
    struct gesture g = {0};
    if (gesture_event(&g, 1, 0, 0) != ACT_NONE)
        return 1;
    if (gesture_event(&g, 0, 100, 0) != ACT_CLICK)
        return 2;
    g = (struct gesture){0};
    if (gesture_event(&g, 1, 0, 1) != ACT_NONE)
        return 3;
    if (gesture_event(&g, 0, 100, 1) != ACT_NONE)
        return 4;
    if (gesture_timeout(&g, 100) != DOUBLE_MS)
        return 5;
    if (gesture_due(&g, 449) != ACT_NONE)
        return 6;
    if (gesture_due(&g, 450) != ACT_CLICK)
        return 7;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    if (gesture_timeout(&g, 0) != LONG_MS)
        return 8;
    gesture_event(&g, 0, 100, 1);
    gesture_event(&g, 1, 200, 1);
    if (gesture_event(&g, 0, 300, 1) != ACT_DOUBLE)
        return 9;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    if (gesture_due(&g, 999) != ACT_NONE)
        return 10;
    if (gesture_due(&g, 1000) != ACT_LONG)
        return 11;
    if (gesture_event(&g, 0, 1100, 1) != ACT_NONE)
        return 12;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    gesture_event(&g, 0, 100, 1);
    gesture_event(&g, 1, 200, 1);
    if (gesture_event(&g, 0, 500, 1) != ACT_CLICK)
        return 13;
    if (gesture_due(&g, 850) != ACT_CLICK)
        return 14;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    gesture_event(&g, 2, 50, 1);
    gesture_event(&g, 0, 100, 1);
    gesture_event(&g, 1, 300, 1);
    if (gesture_event(&g, 0, 450, 1) != ACT_DOUBLE)
        return 15;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    gesture_event(&g, 0, 100, 1);
    gesture_event(&g, 1, 451, 1);
    if (gesture_event(&g, 0, 500, 1) != ACT_CLICK)
        return 16;
    if (gesture_timeout(&g, 500) != DOUBLE_MS)
        return 17;
    if (gesture_due(&g, 850) != ACT_CLICK)
        return 18;
    g = (struct gesture){0};
    if (gesture_event(&g, 0, 0, 1) != ACT_NONE)
        return 19;
    if (gesture_timeout(&g, 0) != -1)
        return 20;
    g = (struct gesture){0};
    gesture_event(&g, 1, 0, 1);
    gesture_event(&g, 0, 100, 1);
    gesture_event(&g, 1, 200, 1);
    if (gesture_due(&g, 1200) != ACT_CLICK)
        return 21;
    if (gesture_due(&g, 1200) != ACT_LONG)
        return 22;
    return 0;
}

int main(void)
{
    return test_gestures();
}

#else

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define MODULE_DIR "/data/adb/modules/Easy_Key"
#define CONFIG_FILE MODULE_DIR "/config.ini"
#define INPUT_DEVICE "/dev/input/event0"
#define COMMAND_SIZE 1024

static char click_cmd[COMMAND_SIZE];
static char long_cmd[COMMAND_SIZE];
static char double_cmd[COMMAND_SIZE];

static void set_command(char *dst, const char *src, size_t length)
{
    if (length >= COMMAND_SIZE)
        length = COMMAND_SIZE - 1;
    memcpy(dst, src, length);
    dst[length] = 0;
}

static void load_config(void)
{
    char data[COMMAND_SIZE * 3 + 24];
    char *line;
    char *p;
    char *end;
    ssize_t size;
    size_t total = 0;
    int fd;
    static const char default_click[] = "cmd activity start -n com.parallelc.micts/.ui.activity.MainActivity";
    static const char default_long[] = "input keyevent KEYCODE_HOME";
    set_command(click_cmd, default_click, sizeof(default_click) - 1);
    set_command(long_cmd, default_long, sizeof(default_long) - 1);
    double_cmd[0] = 0;
    fd = open(CONFIG_FILE, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    while (total < sizeof(data)) {
        size = read(fd, data + total, sizeof(data) - total);
        if (size > 0) {
            total += (size_t)size;
            continue;
        }
        if (size < 0 && errno == EINTR)
            continue;
        break;
    }
    close(fd);
    p = data;
    end = data + total;
    while (p < end) {
        size_t length;
        line = p;
        while (p < end && *p != '\r' && *p != '\n')
            p++;
        length = (size_t)(p - line);
        while (p < end && (*p == '\r' || *p == '\n'))
            p++;
        if (length >= 6 && !memcmp(line, "click=", 6))
            set_command(click_cmd, line + 6, length - 6);
        else if (length >= 5 && !memcmp(line, "long=", 5))
            set_command(long_cmd, line + 5, length - 5);
        else if (length >= 7 && !memcmp(line, "double=", 7))
            set_command(double_cmd, line + 7, length - 7);
    }
}

static int64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int64_t input_ms(const struct input_event *event, int monotonic)
{
    if (!monotonic)
        return now_ms();
    return (int64_t)event->time.tv_sec * 1000 + event->time.tv_usec / 1000;
}

static void run_action(int action)
{
    const char *cmd = action == ACT_CLICK ? click_cmd : action == ACT_LONG ? long_cmd : double_cmd;
    pid_t pid;
    if (!*cmd)
        return;
    pid = vfork();
    if (pid == 0) {
        execl("/system/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(127);
    }
}

static void run_due(struct gesture *gesture, int64_t now)
{
    int action;
    while ((action = gesture_due(gesture, now)) != ACT_NONE)
        run_action(action);
}

static void reload_events(int fd)
{
    union {
        struct inotify_event event;
        char data[512];
    } buffer;
    int reload = 0;
    ssize_t size;
    for (;;) {
        char *p;
        size = read(fd, buffer.data, sizeof(buffer.data));
        if (size <= 0)
            break;
        for (p = buffer.data; p < buffer.data + size;) {
            struct inotify_event *event = (struct inotify_event *)p;
            if (event->len && !strcmp(event->name, "config.ini"))
                reload = 1;
            p += sizeof(*event) + event->len;
        }
    }
    if (reload)
        load_config();
}

static int process_input(int fd, struct gesture *gesture, int monotonic)
{
    struct input_event events[16];
    ssize_t size;
    for (;;) {
        size_t count;
        size_t i;
        size = read(fd, events, sizeof(events));
        if (size < 0)
            return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
        if (size == 0 || size % sizeof(events[0]))
            return 0;
        count = (size_t)size / sizeof(events[0]);
        for (i = 0; i < count; i++) {
            int action;
            int64_t when;
            if (events[i].type != EV_KEY || events[i].code != KEY_CODE)
                continue;
            when = input_ms(&events[i], monotonic);
            action = gesture_event(gesture, events[i].value, when, double_cmd[0] != 0);
            if (action != ACT_NONE)
                run_action(action);
        }
    }
}

int main(void)
{
    struct gesture gesture = {0};
    struct pollfd fds[2];
    clockid_t clock_id = CLOCK_MONOTONIC;
    int input_fd;
    int notify_fd;
    int monotonic;
    int ready;
    nfds_t count;
    signal(SIGCHLD, SIG_IGN);
    input_fd = open(INPUT_DEVICE, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (input_fd < 0)
        return 1;
    monotonic = ioctl(input_fd, EVIOCSCLOCKID, &clock_id) == 0;
    notify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (notify_fd >= 0 && inotify_add_watch(notify_fd, MODULE_DIR, IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_DELETE) < 0) {
        close(notify_fd);
        notify_fd = -1;
    }
    load_config();
    fds[0].fd = input_fd;
    fds[0].events = POLLIN;
    fds[1].fd = notify_fd;
    fds[1].events = POLLIN;
    count = notify_fd >= 0 ? 2 : 1;
    for (;;) {
        int64_t now = now_ms();
        ready = poll(fds, count, gesture_timeout(&gesture, now));
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready > 0 && count == 2 && (fds[1].revents & POLLIN))
            reload_events(notify_fd);
        if (ready > 0 && (fds[0].revents & POLLIN) && !process_input(input_fd, &gesture, monotonic))
            break;
        run_due(&gesture, now_ms());
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
    }
    close(input_fd);
    if (notify_fd >= 0)
        close(notify_fd);
    return 1;
}

#endif
