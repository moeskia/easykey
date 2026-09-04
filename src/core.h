#ifndef EASYKEY_CORE_H
#define EASYKEY_CORE_H

#include <stddef.h>
#include <stdint.h>

enum { ACT_NONE, ACT_CLICK, ACT_LONG, ACT_DOUBLE };
enum { LONG_MS = 1000, DOUBLE_MS = 350 };
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

void set_command(struct command *command, const char *src, size_t length);
int gesture_due(struct gesture *g, int64_t now);
int gesture_event(struct gesture *g, int value, int64_t now, int double_enabled);
int gesture_timeout(const struct gesture *g, int64_t now);

#endif
