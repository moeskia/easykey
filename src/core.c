#include "core.h"
#include <string.h>

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

int set_command(struct command *command, const char *src, size_t length)
{
    size_t i;
    if (length >= COMMAND_SIZE)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c < 32 && c != '\t') || c == 127)
            return 0;
    }
    memcpy(command->text, src, length);
    command->text[length] = 0;
    prepare_command(command);
    return 1;
}

int parse_config(struct config *config, const char *data, size_t length)
{
    struct config next = {0};
    const char *p = data;
    const char *end = data + length;
    unsigned seen = 0;
    if (length > CONFIG_SIZE)
        return 0;
    while (p < end) {
        const char *line = p;
        struct command *command;
        size_t size;
        size_t prefix;
        unsigned key;
        while (p < end && *p != '\r' && *p != '\n')
            p++;
        size = (size_t)(p - line);
        while (p < end && (*p == '\r' || *p == '\n'))
            p++;
        if (!size)
            continue;
        if (size >= 6 && !memcmp(line, "click=", 6)) {
            command = &next.click;
            prefix = 6;
            key = 1;
        } else if (size >= 5 && !memcmp(line, "long=", 5)) {
            command = &next.long_press;
            prefix = 5;
            key = 2;
        } else if (size >= 7 && !memcmp(line, "double=", 7)) {
            command = &next.double_click;
            prefix = 7;
            key = 4;
        } else {
            return 0;
        }
        if ((seen & key) || !set_command(command, line + prefix, size - prefix))
            return 0;
        seen |= key;
    }
    *config = next;
    return 1;
}

int gesture_due(struct gesture *g, int64_t now)
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

int gesture_event(struct gesture *g, int value, int64_t now, int double_enabled)
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

int gesture_timeout(const struct gesture *g, int64_t now)
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
