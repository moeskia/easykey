#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { ACT_NONE, ACT_CLICK, ACT_LONG, ACT_DOUBLE };
enum { KEY_CODE = 735, LONG_MS = 1000, DOUBLE_MS = 350 };
enum { COMMAND_SIZE = 1024, COMMAND_ARGS = 32 };
enum { EXEC_SHELL, EXEC_CMD, EXEC_INPUT, EXEC_SERVICE, EXEC_SH };

struct gesture {
    int down;
    int long_fired;
    int pending;
    int64_t down_at;
    int64_t first_up;
};

struct command {
    char text[COMMAND_SIZE];
    uint16_t offsets[COMMAND_ARGS];
    uint8_t argc;
    uint8_t program;
};

static int shell_syntax(unsigned char c)
{
    if (c < 32 || c == 127)
        return c != '\t';
    switch (c) {
    case '|':
    case '&':
    case ';':
    case '<':
    case '>':
    case '(':
    case ')':
    case '$':
    case '`':
    case '\\':
    case '"':
    case '\'':
    case '{':
    case '}':
    case '[':
    case ']':
    case '*':
    case '?':
    case '!':
    case '~':
    case '#':
    case '=':
        return 1;
    default:
        return 0;
    }
}

static uint8_t command_program(const char *word, size_t length)
{
    if ((length == 3 && !memcmp(word, "cmd", 3)) ||
        (length == 15 && !memcmp(word, "/system/bin/cmd", 15)))
        return EXEC_CMD;
    if ((length == 5 && !memcmp(word, "input", 5)) ||
        (length == 17 && !memcmp(word, "/system/bin/input", 17)))
        return EXEC_INPUT;
    if ((length == 7 && !memcmp(word, "service", 7)) ||
        (length == 19 && !memcmp(word, "/system/bin/service", 19)))
        return EXEC_SERVICE;
    if (length == 14 && !memcmp(word, "/system/bin/sh", 14))
        return EXEC_SH;
    return EXEC_SHELL;
}

static void prepare_command(struct command *command)
{
    size_t argc = 0;
    size_t first = 0;
    size_t first_length = 0;
    size_t i;
    int word = 0;
    uint8_t program;
    command->argc = 0;
    command->program = EXEC_SHELL;
    for (i = 0; command->text[i]; i++) {
        unsigned char c = (unsigned char)command->text[i];
        if (c == ' ' || c == '\t') {
            if (word) {
                if (argc == 0)
                    first_length = i - first;
                argc++;
                word = 0;
            }
            continue;
        }
        if (shell_syntax(c))
            return;
        if (!word) {
            if (argc >= COMMAND_ARGS)
                return;
            if (argc == 0)
                first = i;
            word = 1;
        }
    }
    if (word) {
        if (argc == 0)
            first_length = i - first;
        argc++;
    }
    if (!argc || argc > COMMAND_ARGS)
        return;
    program = command_program(command->text + first, first_length);
    if (program == EXEC_SHELL)
        return;
    argc = 0;
    for (i = 0; command->text[i];) {
        while (command->text[i] == ' ' || command->text[i] == '\t')
            command->text[i++] = 0;
        if (!command->text[i])
            break;
        command->offsets[argc++] = (uint16_t)i;
        while (command->text[i] && command->text[i] != ' ' && command->text[i] != '\t')
            i++;
    }
    command->argc = (uint8_t)argc;
    command->program = program;
}

static void set_command(struct command *command, const char *src, size_t length)
{
    if (length >= COMMAND_SIZE)
        length = COMMAND_SIZE - 1;
    memcpy(command->text, src, length);
    command->text[length] = 0;
    prepare_command(command);
}

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
    struct command command;
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
    set_command(&command, "cmd activity start -n a/b", sizeof("cmd activity start -n a/b") - 1);
    if (command.program != EXEC_CMD || command.argc != 5)
        return 23;
    if (strcmp(command.text + command.offsets[1], "activity"))
        return 24;
    if (strcmp(command.text + command.offsets[4], "a/b"))
        return 25;
    set_command(&command, "  input\tkeyevent KEYCODE_HOME  ", sizeof("  input\tkeyevent KEYCODE_HOME  ") - 1);
    if (command.program != EXEC_INPUT || command.argc != 3)
        return 26;
    if (strcmp(command.text + command.offsets[2], "KEYCODE_HOME"))
        return 27;
    set_command(&command, "service call test 1", sizeof("service call test 1") - 1);
    if (command.program != EXEC_SERVICE || command.argc != 4)
        return 28;
    set_command(&command, "/system/bin/sh /data/test.sh", sizeof("/system/bin/sh /data/test.sh") - 1);
    if (command.program != EXEC_SH || command.argc != 2)
        return 29;
    set_command(&command, "cmd activity --es key \"hello world\"", sizeof("cmd activity --es key \"hello world\"") - 1);
    if (command.program != EXEC_SHELL || strcmp(command.text, "cmd activity --es key \"hello world\""))
        return 30;
    set_command(&command, "cmd activity | input keyevent 3", sizeof("cmd activity | input keyevent 3") - 1);
    if (command.program != EXEC_SHELL || strcmp(command.text, "cmd activity | input keyevent 3"))
        return 31;
    set_command(&command, "cmd $ACTION", sizeof("cmd $ACTION") - 1);
    if (command.program != EXEC_SHELL || strcmp(command.text, "cmd $ACTION"))
        return 32;
    set_command(&command, "cmd value=x", sizeof("cmd value=x") - 1);
    if (command.program != EXEC_SHELL || strcmp(command.text, "cmd value=x"))
        return 33;
    set_command(&command, "unknown arg", sizeof("unknown arg") - 1);
    if (command.program != EXEC_SHELL || strcmp(command.text, "unknown arg"))
        return 34;
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
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define MODULE_DIR "/data/adb/modules/Easy_Key"
#define CONFIG_FILE MODULE_DIR "/config.ini"
#define INPUT_SCAN_MAX 64

static struct command click_cmd;
static struct command long_cmd;
static struct command double_cmd;

static void load_config(void)
{
    char data[COMMAND_SIZE * 3 + 24];
    char *line;
    char *p;
    char *end;
    ssize_t size;
    size_t total = 0;
    int fd;
    static const char default_click[] = "/system/bin/sh /data/adb/modules/Easy_Key/ind/torch.sh";
    static const char default_long[] = "input keyevent KEYCODE_HOME";
    set_command(&click_cmd, default_click, sizeof(default_click) - 1);
    set_command(&long_cmd, default_long, sizeof(default_long) - 1);
    set_command(&double_cmd, "", 0);
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
            set_command(&click_cmd, line + 6, length - 6);
        else if (length >= 5 && !memcmp(line, "long=", 5))
            set_command(&long_cmd, line + 5, length - 5);
        else if (length >= 7 && !memcmp(line, "double=", 7))
            set_command(&double_cmd, line + 7, length - 7);
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
    char path[32];
    int fd;
    int index;
    int length;
    for (index = 0; index < INPUT_SCAN_MAX; index++) {
        length = snprintf(path, sizeof(path), "/dev/input/event%d", index);
        if (length < 0 || (size_t)length >= sizeof(path))
            continue;
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
    struct command *command = action == ACT_CLICK ? &click_cmd : action == ACT_LONG ? &long_cmd : &double_cmd;
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
            action = gesture_event(gesture, events[i].value, when, command_enabled(&double_cmd));
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

#endif
