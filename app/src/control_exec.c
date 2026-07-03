#include "control_exec.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "control_msg.h"
#include "options.h"
#include "util/acksync.h"
#include "util/log.h"
#include "util/thread.h"

// Cap client-provided durations so that a single command cannot block the
// executor (and, in daemon mode, session teardown) for hours
#define SC_CONTROL_EXEC_MAX_STEP_MS (60 * 1000)

struct sc_finger_action {
    int x1, y1; // start
    int x2, y2; // end (same as start for click)
    int duration; // ms
    bool is_swipe;
};

static int
sc_parse_duration_ms(const char *s, int def) {
    int ms = s ? atoi(s) : def;
    if (ms < 0) {
        ms = 0;
    } else if (ms > SC_CONTROL_EXEC_MAX_STEP_MS) {
        LOGW("Duration capped to %d ms", SC_CONTROL_EXEC_MAX_STEP_MS);
        ms = SC_CONTROL_EXEC_MAX_STEP_MS;
    }
    return ms;
}

static bool
sc_parse_touch_cmd(const char *cmd_str, struct sc_finger_action *action) {
    char *cmd = strdup(cmd_str);
    if (!cmd) {
        LOG_OOM();
        return false;
    }

    char *saveptr;
    char *token = strtok_r(cmd, " ", &saveptr);
    if (!token) {
        LOGE("Invalid control command (empty)");
        free(cmd);
        return false;
    }

    if (strcmp(token, "click") == 0) {
        char *x_str = strtok_r(NULL, " ", &saveptr);
        char *y_str = strtok_r(NULL, " ", &saveptr);
        char *dur_str = strtok_r(NULL, " ", &saveptr);

        if (!x_str || !y_str || strtok_r(NULL, " ", &saveptr)) {
            LOGE("Usage: click <x> <y> [duration_ms]");
            free(cmd);
            return false;
        }

        action->x1 = action->x2 = atoi(x_str);
        action->y1 = action->y2 = atoi(y_str);
        action->duration = sc_parse_duration_ms(dur_str, 100);
        action->is_swipe = false;
    } else if (strcmp(token, "swipe") == 0) {
        char *x1_str = strtok_r(NULL, " ", &saveptr);
        char *y1_str = strtok_r(NULL, " ", &saveptr);
        char *x2_str = strtok_r(NULL, " ", &saveptr);
        char *y2_str = strtok_r(NULL, " ", &saveptr);
        char *dur_str = strtok_r(NULL, " ", &saveptr);

        if (!x1_str || !y1_str || !x2_str || !y2_str
                || strtok_r(NULL, " ", &saveptr)) {
            LOGE("Usage: swipe <x1> <y1> <x2> <y2> [duration_ms]");
            free(cmd);
            return false;
        }

        action->x1 = atoi(x1_str);
        action->y1 = atoi(y1_str);
        action->x2 = atoi(x2_str);
        action->y2 = atoi(y2_str);
        action->duration = sc_parse_duration_ms(dur_str, 300);
        action->is_swipe = true;
    } else {
        LOGE("Unknown touch command: %s", token);
        free(cmd);
        return false;
    }

    free(cmd);
    return true;
}

static bool
sc_execute_touch_cmds(struct sc_controller *controller,
                      struct sc_size screen_size,
                      const char *const *cmds, unsigned count,
                      uint64_t base_pointer_id) {
    struct sc_finger_action actions[SC_MAX_CONTROL_CMDS];

    for (unsigned i = 0; i < count; i++) {
        if (!sc_parse_touch_cmd(cmds[i], &actions[i])) {
            return false;
        }
    }

    int max_duration = 0;
    for (unsigned i = 0; i < count; i++) {
        if (actions[i].duration > max_duration) {
            max_duration = actions[i].duration;
        }
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg.inject_touch_event.position.screen_size = screen_size;
    msg.inject_touch_event.action_button = 0;
    msg.inject_touch_event.buttons = 0;

    // Send DOWN for all fingers
    for (unsigned i = 0; i < count; i++) {
        msg.inject_touch_event.action = AMOTION_EVENT_ACTION_DOWN;
        msg.inject_touch_event.pointer_id = base_pointer_id + i;
        msg.inject_touch_event.position.point.x = actions[i].x1;
        msg.inject_touch_event.position.point.y = actions[i].y1;
        msg.inject_touch_event.pressure = 1.0f;

        if (!sc_controller_push_msg(controller, &msg)) {
            LOGW("Could not send touch down for finger %u", i);
        }

        if (i + 1 < count) {
            SDL_Delay(5);
        }
    }

    bool active[SC_MAX_CONTROL_CMDS];
    for (unsigned i = 0; i < count; i++) {
        active[i] = true;
    }

    int step_ms = 10;
    for (int t = step_ms; t <= max_duration + step_ms; t += step_ms) {
        SDL_Delay(step_ms);

        for (unsigned i = 0; i < count; i++) {
            if (!active[i]) {
                continue;
            }

            if (t >= actions[i].duration) {
                // Send UP
                msg.inject_touch_event.action = AMOTION_EVENT_ACTION_UP;
                msg.inject_touch_event.pointer_id = base_pointer_id + i;
                msg.inject_touch_event.position.point.x = actions[i].x2;
                msg.inject_touch_event.position.point.y = actions[i].y2;
                msg.inject_touch_event.pressure = 0.0f;

                if (!sc_controller_push_msg(controller, &msg)) {
                    LOGW("Could not send touch up for finger %u", i);
                }

                active[i] = false;
            } else if (actions[i].is_swipe) {
                // Interpolate position
                float progress = (float)t / actions[i].duration;
                int x = actions[i].x1
                    + (int)((actions[i].x2 - actions[i].x1) * progress);
                int y = actions[i].y1
                    + (int)((actions[i].y2 - actions[i].y1) * progress);

                msg.inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
                msg.inject_touch_event.pointer_id = base_pointer_id + i;
                msg.inject_touch_event.position.point.x = x;
                msg.inject_touch_event.position.point.y = y;
                msg.inject_touch_event.pressure = 1.0f;

                if (!sc_controller_push_msg(controller, &msg)) {
                    LOGW("Could not send touch move for finger %u", i);
                }
            }
        }
    }

    return true;
}

static bool
sc_execute_continuous_swipe(struct sc_controller *controller,
                            struct sc_size screen_size,
                            const struct sc_finger_action *segments,
                            unsigned count, uint64_t pointer_id) {
    int total_duration = 0;
    for (unsigned i = 0; i < count; i++) {
        total_duration += segments[i].duration;
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg.inject_touch_event.position.screen_size = screen_size;
    msg.inject_touch_event.action_button = 0;
    msg.inject_touch_event.buttons = 0;

    // DOWN at start of first segment
    msg.inject_touch_event.action = AMOTION_EVENT_ACTION_DOWN;
    msg.inject_touch_event.pointer_id = pointer_id;
    msg.inject_touch_event.position.point.x = segments[0].x1;
    msg.inject_touch_event.position.point.y = segments[0].y1;
    msg.inject_touch_event.pressure = 1.0f;

    if (!sc_controller_push_msg(controller, &msg)) {
        LOGW("Could not send touch down for continuous swipe");
    }

    int step_ms = 10;
    for (int t = step_ms; t <= total_duration; t += step_ms) {
        SDL_Delay(step_ms);

        // Find which segment we're in
        int cumulative = 0;
        unsigned seg = 0;
        for (unsigned i = 0; i < count; i++) {
            if (t <= cumulative + segments[i].duration) {
                seg = i;
                break;
            }
            cumulative += segments[i].duration;
        }

        float progress = (float)(t - cumulative) / segments[seg].duration;
        int x = segments[seg].x1
            + (int)((segments[seg].x2 - segments[seg].x1) * progress);
        int y = segments[seg].y1
            + (int)((segments[seg].y2 - segments[seg].y1) * progress);

        msg.inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
        msg.inject_touch_event.pointer_id = pointer_id;
        msg.inject_touch_event.position.point.x = x;
        msg.inject_touch_event.position.point.y = y;
        msg.inject_touch_event.pressure = 1.0f;

        if (!sc_controller_push_msg(controller, &msg)) {
            LOGW("Could not send touch move for continuous swipe");
        }
    }

    // UP at end of last segment
    msg.inject_touch_event.action = AMOTION_EVENT_ACTION_UP;
    msg.inject_touch_event.pointer_id = pointer_id;
    msg.inject_touch_event.position.point.x = segments[count - 1].x2;
    msg.inject_touch_event.position.point.y = segments[count - 1].y2;
    msg.inject_touch_event.pressure = 0.0f;

    if (!sc_controller_push_msg(controller, &msg)) {
        LOGW("Could not send touch up for continuous swipe");
    }

    return true;
}

static bool
sc_execute_input_cmd(struct sc_controller *controller, const char *cmd_str) {
    // Skip "input " prefix
    const char *text = cmd_str + 6; // strlen("input ")

    // Strip optional surrounding quotes
    size_t len = strlen(text);
    char *text_dup;
    if (len >= 2
            && ((text[0] == '\'' && text[len - 1] == '\'')
             || (text[0] == '"' && text[len - 1] == '"'))) {
        text_dup = strndup(text + 1, len - 2);
    } else {
        text_dup = strdup(text);
    }
    if (!text_dup) {
        LOG_OOM();
        return false;
    }

    // Log before pushing: the message owns the text, and the controller
    // thread may free it as soon as it is queued
    LOGI("Injecting text: %s", text_dup);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_SET_CLIPBOARD;
    msg.set_clipboard.sequence = SC_SEQUENCE_INVALID;
    msg.set_clipboard.text = text_dup;
    msg.set_clipboard.paste = true;

    if (!sc_controller_push_msg(controller, &msg)) {
        free(text_dup);
        LOGE("Could not inject text");
        return false;
    }

    return true;
}

static bool
sc_execute_single_step(struct sc_controller *controller,
                       struct sc_size screen_size, const char *cmd,
                       uint64_t pointer_id) {
    // Trim leading whitespace
    while (*cmd == ' ') {
        cmd++;
    }

    if (!*cmd) {
        return true; // empty step, skip
    }

    if (!strncmp(cmd, "sleep ", 6) || !strcmp(cmd, "sleep")) {
        int ms = 0;
        if (strlen(cmd) > 6) {
            ms = sc_parse_duration_ms(cmd + 6, 0);
        }
        if (ms > 0) {
            SDL_Delay(ms);
        }
        return true;
    }

    if (!strncmp(cmd, "input ", 6)) {
        return sc_execute_input_cmd(controller, cmd);
    }

    if (!strncmp(cmd, "click ", 6) || !strncmp(cmd, "swipe ", 6)) {
        return sc_execute_touch_cmds(controller, screen_size, &cmd, 1,
                                     pointer_id);
    }

    LOGE("Unknown control command: %s", cmd);
    return false;
}

// Execute a single --control arg: if it contains "&&", split and run steps
// serially; consecutive connected swipes without sleep are merged into one
// continuous stroke.
static bool
sc_execute_one_control_arg(struct sc_controller *controller,
                           struct sc_size screen_size, const char *arg,
                           uint64_t pointer_id) {
    if (!strstr(arg, "&&")) {
        // No "&&": execute as a single command
        return sc_execute_single_step(controller, screen_size, arg,
                                      pointer_id);
    }

    // Split by "&&" into steps array
    char *dup = strdup(arg);
    if (!dup) {
        LOG_OOM();
        return false;
    }

    char *steps[SC_MAX_CONTROL_CMDS];
    unsigned step_count = 0;

    char *remaining = dup;
    while (remaining && *remaining && step_count < SC_MAX_CONTROL_CMDS) {
        char *sep = strstr(remaining, "&&");
        if (sep) {
            *sep = '\0';
        }

        // Trim whitespace
        char *step = remaining;
        while (*step == ' ') {
            step++;
        }
        size_t slen = strlen(step);
        while (slen > 0 && step[slen - 1] == ' ') {
            step[--slen] = '\0';
        }

        if (*step) {
            steps[step_count++] = step;
        }

        if (sep) {
            remaining = sep + 2;
        } else {
            break;
        }
    }

    bool ok = true;
    unsigned i = 0;

    while (i < step_count && ok) {
        // Check if this step is a swipe
        struct sc_finger_action action;
        if (!strncmp(steps[i], "swipe ", 6)
                && sc_parse_touch_cmd(steps[i], &action)
                && action.is_swipe) {

            // Look ahead for consecutive connected swipes
            struct sc_finger_action segments[SC_MAX_CONTROL_CMDS];
            unsigned seg_count = 0;
            segments[seg_count++] = action;

            unsigned j = i + 1;
            while (j < step_count && seg_count < SC_MAX_CONTROL_CMDS) {
                struct sc_finger_action next;
                if (strncmp(steps[j], "swipe ", 6) != 0
                        || !sc_parse_touch_cmd(steps[j], &next)
                        || !next.is_swipe) {
                    break;
                }
                // Check if connected: end of previous == start of next
                if (segments[seg_count - 1].x2 != next.x1
                        || segments[seg_count - 1].y2 != next.y1) {
                    break;
                }
                segments[seg_count++] = next;
                j++;
            }

            if (seg_count > 1) {
                ok = sc_execute_continuous_swipe(controller, screen_size,
                                                 segments, seg_count,
                                                 pointer_id);
            } else {
                ok = sc_execute_single_step(controller, screen_size, steps[i],
                                            pointer_id);
            }
            i = j;
        } else {
            ok = sc_execute_single_step(controller, screen_size, steps[i],
                                        pointer_id);
            i++;
        }
    }

    free(dup);
    return ok;
}

struct sc_control_thread_args {
    struct sc_controller *controller;
    struct sc_size screen_size;
    const char *arg;
    uint64_t pointer_id;
    bool ok;
};

static int
sc_run_control_thread(void *data) {
    struct sc_control_thread_args *args = data;
    args->ok = sc_execute_one_control_arg(args->controller, args->screen_size,
                                          args->arg, args->pointer_id);
    return 0;
}

bool
sc_control_exec_run(struct sc_controller *controller,
                    struct sc_size screen_size,
                    const char *const *cmds, unsigned count,
                    uint64_t pointer_id_base) {
    if (count == 1) {
        // Single --control arg: execute directly
        return sc_execute_one_control_arg(controller, screen_size, cmds[0],
                                          pointer_id_base);
    }

    // Check if any --control arg contains "&&"
    bool any_has_chain = false;
    for (unsigned i = 0; i < count; i++) {
        if (strstr(cmds[i], "&&")) {
            any_has_chain = true;
            break;
        }
    }

    if (!any_has_chain) {
        // No "&&" anywhere: use the multi-touch parallel model
        // (coordinated pointer_ids for simultaneous fingers)
        const char *touch_cmds[SC_MAX_CONTROL_CMDS];
        unsigned touch_count = 0;

        for (unsigned i = 0; i < count; i++) {
            const char *cmd = cmds[i];
            if (!strncmp(cmd, "input ", 6)) {
                if (!sc_execute_input_cmd(controller, cmd)) {
                    return false;
                }
            } else if (!strncmp(cmd, "click ", 6)
                    || !strncmp(cmd, "swipe ", 6)) {
                touch_cmds[touch_count++] = cmd;
            } else {
                LOGE("Unknown control command: %s", cmd);
                return false;
            }
        }

        if (touch_count > 0) {
            if (!sc_execute_touch_cmds(controller, screen_size, touch_cmds,
                                       touch_count, pointer_id_base)) {
                return false;
            }
        }

        return true;
    }

    // At least one arg has "&&": each --control runs in its own thread
    // (parallel between args, serial within each arg's "&&" chain).
    struct sc_control_thread_args thread_args[SC_MAX_CONTROL_CMDS];
    sc_thread threads[SC_MAX_CONTROL_CMDS];

    for (unsigned i = 0; i < count; i++) {
        thread_args[i].controller = controller;
        thread_args[i].screen_size = screen_size;
        thread_args[i].arg = cmds[i];
        thread_args[i].pointer_id = pointer_id_base + i;
        thread_args[i].ok = false;

        if (!sc_thread_create(&threads[i], sc_run_control_thread,
                              "scrcpy-ctrl", &thread_args[i])) {
            LOGE("Could not create control thread %u", i);
            for (unsigned j = 0; j < i; j++) {
                sc_thread_join(&threads[j], NULL);
            }
            return false;
        }
    }

    bool ok = true;
    for (unsigned i = 0; i < count; i++) {
        sc_thread_join(&threads[i], NULL);
        if (!thread_args[i].ok) {
            ok = false;
        }
    }

    return ok;
}
