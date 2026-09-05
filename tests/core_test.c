#include "../src/core.h"
#include <string.h>
#include <assert.h>

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

static void test_config(void)
{
    struct config config = {0};
    struct config saved;
    struct command command = {0};
    char limit[COMMAND_SIZE + 1];
    char data[CONFIG_SIZE + 1];
    const char initial[] = "click=  echo \"a  b\"  \r\nlong=\r\ndouble=echo again";
    const char *invalid[] = {"click=echo changed\nclick=echo duplicate", "click=echo changed\nlong =echo invalid", "click=echo changed\nunknown=bad", "click=echo changed\nnot-a-key", "click=echo\x01"};
    size_t i;
    assert(parse_config(&config, initial, sizeof(initial) - 1));
    assert(!strcmp(config.click.text, "  echo \"a  b\"  "));
    assert(!config.long_press.text[0]);
    assert(!strcmp(config.double_click.text, "echo again"));
    saved = config;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(!parse_config(&config, invalid[i], strlen(invalid[i])));
        assert(!memcmp(&config, &saved, sizeof(config)));
    }
    assert(!parse_config(&config, "click=echo\0bad", sizeof("click=echo\0bad") - 1));
    assert(!memcmp(&config, &saved, sizeof(config)));
    memset(data, '\n', sizeof(data));
    assert(!parse_config(&config, data, sizeof(data)));
    assert(!memcmp(&config, &saved, sizeof(config)));
    memset(limit, 'x', sizeof(limit));
    assert(set_command(&command, limit, COMMAND_SIZE - 1));
    assert(strlen(command.text) == COMMAND_SIZE - 1);
    assert(!set_command(&command, limit, COMMAND_SIZE));
    assert(strlen(command.text) == COMMAND_SIZE - 1);
    memcpy(data, "click=", 6);
    memcpy(data + 6, limit, COMMAND_SIZE);
    assert(!parse_config(&config, data, COMMAND_SIZE + 6));
    assert(!memcmp(&config, &saved, sizeof(config)));
    assert(parse_config(&config, data, COMMAND_SIZE + 5));
    assert(strlen(config.click.text) == COMMAND_SIZE - 1);
    assert(!config.long_press.text[0] && !config.double_click.text[0]);
    assert(parse_config(&config, "double=echo only", sizeof("double=echo only") - 1));
    assert(!config.click.text[0] && !config.long_press.text[0]);
    assert(!strcmp(config.double_click.text, "echo only"));
    assert(parse_config(&config, "", 0));
    assert(!config.click.text[0] && !config.long_press.text[0] && !config.double_click.text[0]);
    assert(!set_command(&command, "echo\nchanged", sizeof("echo\nchanged") - 1));
}

int main(void)
{
    int result = test_gestures();
    if (!result)
        test_config();
    return result;
}
