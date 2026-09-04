#include "../src/core.h"
#include <string.h>

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
