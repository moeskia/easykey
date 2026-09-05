#include "core.h"
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

enum { KEY_CODE = 735 };

#define MODULE_DIR "/data/adb/modules/Easy_Key"
#define CONFIG_FILE MODULE_DIR "/config.ini"
#define INPUT_SCAN_MAX 64

static struct config config;

static void load_config(void)
{
    char data[CONFIG_SIZE + 1];
    ssize_t size = 0;
    size_t total = 0;
    int fd = open(CONFIG_FILE, O_RDONLY | O_CLOEXEC);
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
    if (size == 0)
        parse_config(&config, data, total);
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

static const char *direct_path(uint8_t program)
{
    switch (program) {
    case EXEC_CMD:
        return "/system/bin/cmd";
    case EXEC_INPUT:
        return "/system/bin/input";
    case EXEC_SERVICE:
        return "/system/bin/service";
    case EXEC_SH:
        return "/system/bin/sh";
    default:
        return 0;
    }
}

static int command_enabled(const struct command *command)
{
    return command->argc || command->text[0];
}

static int input_has_key(int fd)
{
    unsigned char bits[(KEY_MAX + 8) / 8] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0)
        return 0;
    return (bits[KEY_CODE / 8] & (1u << (KEY_CODE % 8))) != 0;
}

static int open_input_device(void)
{
    static const char prefix[] = "/dev/input/event";
    char digits[3];
    char path[sizeof(prefix) + sizeof(digits)];
    int fd;
    int index;
    for (index = 0; index < INPUT_SCAN_MAX; index++) {
        if (index >= 10) {
            digits[0] = (char)('0' + index / 10);
            digits[1] = (char)('0' + index % 10);
            digits[2] = 0;
        } else {
            digits[0] = (char)('0' + index);
            digits[1] = 0;
        }
        memcpy(path, prefix, sizeof(prefix) - 1);
        memcpy(path + sizeof(prefix) - 1, digits, (size_t)(index >= 10 ? 3 : 2));
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        if (input_has_key(fd))
            return fd;
        close(fd);
    }
    return -1;
}

static void run_action(int action)
{
    struct command *command = action == ACT_CLICK ? &config.click : action == ACT_LONG ? &config.long_press : &config.double_click;
    const char *path = direct_path(command->program);
    char *argv[COMMAND_ARGS + 1];
    size_t i;
    pid_t pid;
    if (!command_enabled(command))
        return;
    if (path) {
        for (i = 0; i < command->argc; i++)
            argv[i] = command->text + command->offsets[i];
        argv[command->argc] = 0;
    }
    pid = vfork();
    if (pid == 0) {
        if (path)
            execv(path, argv);
        else
            execl("/system/bin/sh", "sh", "-c", command->text, (char *)0);
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
        if (size == 0 || (size_t)size % sizeof(events[0]))
            return 0;
        count = (size_t)size / sizeof(events[0]);
        for (i = 0; i < count; i++) {
            int action;
            int64_t when;
            if (events[i].type != EV_KEY || events[i].code != KEY_CODE)
                continue;
            when = input_ms(&events[i], monotonic);
            action = gesture_event(gesture, events[i].value, when, command_enabled(&config.double_click));
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
    for (;;) {
        input_fd = open_input_device();
        if (input_fd >= 0)
            break;
        sleep(1);
    }
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
