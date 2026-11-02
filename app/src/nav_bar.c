#include "nav_bar.h"

#include <assert.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "control_msg.h"
#include "util/log.h"

// Maximum width of a button cell, in window coordinates
#define CELL_MAX_WIDTH 72
// Radius of the icons, in window coordinates
#define ICON_RADIUS 8.f
// Thickness of the icon strokes, in window coordinates
#define STROKE 1.6f

#define PI 3.14159265f

static const SDL_FColor COLOR_BAR = {0.11f, 0.11f, 0.12f, 1.f};
static const SDL_FColor COLOR_HOVER = {0.22f, 0.22f, 0.24f, 1.f};
static const SDL_FColor COLOR_PRESSED = {0.32f, 0.32f, 0.35f, 1.f};
static const SDL_FColor COLOR_ICON = {0.90f, 0.90f, 0.92f, 1.f};

void
sc_nav_bar_init(struct sc_nav_bar *bar, struct sc_controller *controller) {
    assert(controller);
    bar->controller = controller;
    bar->rect = (SDL_FRect) {0, 0, 0, 0};
    bar->hovered = SC_NAV_BAR_BUTTON_NONE;
    bar->pressed = SC_NAV_BAR_BUTTON_NONE;
    bar->pressed_by_touch = false;
    bar->owned_buttons = 0;
    bar->touch_active = false;
    bar->touch_finger = 0;
}

void
sc_nav_bar_set_rect(struct sc_nav_bar *bar, const SDL_FRect *rect) {
    bar->rect = *rect;
}

static float
get_cell_width(const struct sc_nav_bar *bar) {
    float w = bar->rect.w / SC_NAV_BAR_BUTTON_COUNT;
    return w < CELL_MAX_WIDTH ? w : CELL_MAX_WIDTH;
}

// The buttons are centered horizontally
static SDL_FRect
get_cell_rect(const struct sc_nav_bar *bar, enum sc_nav_bar_button button) {
    float cell_w = get_cell_width(bar);
    float x0 = bar->rect.x
             + (bar->rect.w - cell_w * SC_NAV_BAR_BUTTON_COUNT) / 2.f;
    return (SDL_FRect) {
        .x = x0 + cell_w * button,
        .y = bar->rect.y,
        .w = cell_w,
        .h = bar->rect.h,
    };
}

static enum sc_nav_bar_button
get_button_at(const struct sc_nav_bar *bar, float x, float y) {
    if (y < bar->rect.y || y >= bar->rect.y + bar->rect.h) {
        return SC_NAV_BAR_BUTTON_NONE;
    }

    for (int i = 0; i < SC_NAV_BAR_BUTTON_COUNT; ++i) {
        SDL_FRect cell = get_cell_rect(bar, i);
        if (x >= cell.x && x < cell.x + cell.w) {
            return i;
        }
    }

    return SC_NAV_BAR_BUTTON_NONE;
}

static bool
is_in_bar(const struct sc_nav_bar *bar, float x, float y) {
    return x >= bar->rect.x && x < bar->rect.x + bar->rect.w
        && y >= bar->rect.y && y < bar->rect.y + bar->rect.h;
}

static void
push_msg(struct sc_nav_bar *bar, struct sc_control_msg *msg,
         const char *name) {
    if (!sc_controller_push_msg(bar->controller, msg)) {
        LOGW("Could not request '%s'", name);
    }
}

static void
inject_keycode(struct sc_nav_bar *bar, enum android_keycode keycode,
               bool down, const char *name) {
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE;
    msg.inject_keycode.action = down ? AKEY_EVENT_ACTION_DOWN
                                     : AKEY_EVENT_ACTION_UP;
    msg.inject_keycode.keycode = keycode;
    msg.inject_keycode.repeat = 0;
    msg.inject_keycode.metastate = 0;
    push_msg(bar, &msg, name);
}

static void
apply_button(struct sc_nav_bar *bar, enum sc_nav_bar_button button,
             bool down) {
    struct sc_control_msg msg;
    switch (button) {
        case SC_NAV_BAR_BUTTON_BACK:
            // Like the mouse secondary button: also turn the screen on
            msg.type = SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON;
            msg.back_or_screen_on.action = down ? AKEY_EVENT_ACTION_DOWN
                                                : AKEY_EVENT_ACTION_UP;
            push_msg(bar, &msg, "press back or turn screen on");
            break;
        case SC_NAV_BAR_BUTTON_HOME:
            inject_keycode(bar, AKEYCODE_HOME, down, "inject HOME");
            break;
        case SC_NAV_BAR_BUTTON_APP_SWITCH:
            inject_keycode(bar, AKEYCODE_APP_SWITCH, down, "inject APP_SWITCH");
            break;
        case SC_NAV_BAR_BUTTON_VOLUME_DOWN:
            inject_keycode(bar, AKEYCODE_VOLUME_DOWN, down,
                           "inject VOLUME_DOWN");
            break;
        case SC_NAV_BAR_BUTTON_VOLUME_UP:
            inject_keycode(bar, AKEYCODE_VOLUME_UP, down, "inject VOLUME_UP");
            break;
        case SC_NAV_BAR_BUTTON_POWER:
            inject_keycode(bar, AKEYCODE_POWER, down, "inject POWER");
            break;
        case SC_NAV_BAR_BUTTON_ROTATE:
            if (!down) {
                msg.type = SC_CONTROL_MSG_TYPE_ROTATE_DEVICE;
                push_msg(bar, &msg, "rotate device");
            }
            break;
        default:
            assert(!"Unexpected button");
    }
}

// Release the pressed button on the device, if any (on mouse button release
// or focus loss)
static void
sc_nav_bar_release(struct sc_nav_bar *bar) {
    if (bar->pressed != SC_NAV_BAR_BUTTON_NONE) {
        apply_button(bar, bar->pressed, false);
        bar->pressed = SC_NAV_BAR_BUTTON_NONE;
    }
}

void
sc_nav_bar_reset(struct sc_nav_bar *bar) {
    bar->pressed = SC_NAV_BAR_BUTTON_NONE;
    bar->hovered = SC_NAV_BAR_BUTTON_NONE;
    bar->owned_buttons = 0;
    bar->touch_active = false;
}

static bool
press(struct sc_nav_bar *bar, float x, float y, bool paused, bool touch) {
    if (paused || bar->pressed != SC_NAV_BAR_BUTTON_NONE) {
        // Do not start new device actions while paused
        return false;
    }

    enum sc_nav_bar_button button = get_button_at(bar, x, y);
    if (button == SC_NAV_BAR_BUTTON_NONE) {
        return false;
    }

    bar->pressed = button;
    bar->pressed_by_touch = touch;
    apply_button(bar, button, true);
    return true;
}

bool
sc_nav_bar_handle_event(struct sc_nav_bar *bar, const SDL_Event *event,
                        bool paused, bool *render) {
    *render = false;

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if (event->motion.which == SDL_TOUCH_MOUSEID) {
                return false;
            }
            float x = event->motion.x;
            float y = event->motion.y;
            enum sc_nav_bar_button hovered = get_button_at(bar, x, y);
            if (hovered != bar->hovered) {
                bar->hovered = hovered;
                *render = true;
            }
            // Never steal the motion of a button pressed outside the bar (e.g.
            // a drag on the device screen)
            if (event->motion.state & ~bar->owned_buttons) {
                return false;
            }
            return bar->owned_buttons || is_in_bar(bar, x, y);
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (event->button.which == SDL_TOUCH_MOUSEID) {
                return false;
            }
            float x = event->button.x;
            float y = event->button.y;
            if (!is_in_bar(bar, x, y)) {
                return false;
            }
            // The bar owns this button until it is released, wherever it is
            // released
            bar->owned_buttons |= SDL_BUTTON_MASK(event->button.button);
            if (event->button.button == SDL_BUTTON_LEFT
                    && press(bar, x, y, paused, false)) {
                *render = true;
            }
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (event->button.which == SDL_TOUCH_MOUSEID) {
                return false;
            }
            uint32_t mask = SDL_BUTTON_MASK(event->button.button);
            if (!(bar->owned_buttons & mask)) {
                // Pressed outside the bar: the UP belongs to the input manager
                return false;
            }
            bar->owned_buttons &= ~mask;
            if (event->button.button == SDL_BUTTON_LEFT
                    && bar->pressed != SC_NAV_BAR_BUTTON_NONE
                    && !bar->pressed_by_touch) {
                // Release even if the mouse left the button, like a key
                sc_nav_bar_release(bar);
                *render = true;
            }
            return true;
        }
        case SDL_EVENT_FINGER_DOWN: {
            if (SDL_GetTouchDeviceType(event->tfinger.touchID)
                    != SDL_TOUCH_DEVICE_DIRECT) {
                // Only a touchscreen gives positions relative to the window
                return false;
            }
            // Normalized coordinates, the bar spans the whole window width and
            // ends at the bottom of the window
            float x = event->tfinger.x * bar->rect.w;
            float y = event->tfinger.y * (bar->rect.y + bar->rect.h);
            if (bar->touch_active || !is_in_bar(bar, x, y)) {
                return false;
            }
            bar->touch_active = true;
            bar->touch_finger = event->tfinger.fingerID;
            if (press(bar, x, y, paused, true)) {
                *render = true;
            }
            return true;
        }
        case SDL_EVENT_FINGER_MOTION:
            return bar->touch_active
                && event->tfinger.fingerID == bar->touch_finger;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            if (!bar->touch_active
                    || event->tfinger.fingerID != bar->touch_finger) {
                return false;
            }
            bar->touch_active = false;
            if (bar->pressed != SC_NAV_BAR_BUTTON_NONE
                    && bar->pressed_by_touch) {
                sc_nav_bar_release(bar);
                *render = true;
            }
            return true;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (bar->hovered != SC_NAV_BAR_BUTTON_NONE) {
                bar->hovered = SC_NAV_BAR_BUTTON_NONE;
                *render = true;
            }
            return false;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (bar->pressed != SC_NAV_BAR_BUTTON_NONE) {
                sc_nav_bar_release(bar);
                *render = true;
            }
            bar->owned_buttons = 0;
            bar->touch_active = false;
            return false;
        default:
            return false;
    }
}

// Drawing (all coordinates in physical pixels)

// The icons are made of many small triangles: accumulate them, to render them
// with a single SDL_RenderGeometry() call
#define SC_NAV_BAR_MAX_VERTICES 1024
#define SC_NAV_BAR_MAX_INDICES (2 * SC_NAV_BAR_MAX_VERTICES)
struct sc_nav_bar_geometry {
    SDL_Renderer *renderer;
    int vertex_count;
    int index_count;
    SDL_Vertex vertices[SC_NAV_BAR_MAX_VERTICES];
    int indices[SC_NAV_BAR_MAX_INDICES];
};

static void
geometry_flush(struct sc_nav_bar_geometry *g) {
    if (g->index_count) {
        SDL_RenderGeometry(g->renderer, NULL, g->vertices, g->vertex_count,
                           g->indices, g->index_count);
    }
    g->vertex_count = 0;
    g->index_count = 0;
}

// Reserve space for a shape, and return the index of its first vertex
static int
geometry_reserve(struct sc_nav_bar_geometry *g, int vertices, int indices) {
    assert(vertices <= SC_NAV_BAR_MAX_VERTICES);
    assert(indices <= SC_NAV_BAR_MAX_INDICES);
    if (g->vertex_count + vertices > SC_NAV_BAR_MAX_VERTICES
            || g->index_count + indices > SC_NAV_BAR_MAX_INDICES) {
        geometry_flush(g);
    }
    return g->vertex_count;
}

static void
geometry_add_vertex(struct sc_nav_bar_geometry *g, SDL_FPoint position,
                    SDL_FColor color) {
    assert(g->vertex_count < SC_NAV_BAR_MAX_VERTICES);
    g->vertices[g->vertex_count++] =
        (SDL_Vertex) {.position = position, .color = color};
}

static void
geometry_add_triangle(struct sc_nav_bar_geometry *g, int a, int b, int c) {
    assert(g->index_count + 3 <= SC_NAV_BAR_MAX_INDICES);
    g->indices[g->index_count++] = a;
    g->indices[g->index_count++] = b;
    g->indices[g->index_count++] = c;
}

static void
fill_triangle(struct sc_nav_bar_geometry *g, SDL_FPoint a, SDL_FPoint b,
              SDL_FPoint c, SDL_FColor color) {
    int first = geometry_reserve(g, 3, 3);
    geometry_add_vertex(g, a, color);
    geometry_add_vertex(g, b, color);
    geometry_add_vertex(g, c, color);
    geometry_add_triangle(g, first, first + 1, first + 2);
}

static void
fill_disc(struct sc_nav_bar_geometry *g, SDL_FPoint center, float radius,
          SDL_FColor color) {
    enum { SEGMENTS = 12 };
    int first = geometry_reserve(g, SEGMENTS + 1, 3 * SEGMENTS);
    geometry_add_vertex(g, center, color);
    for (int i = 0; i < SEGMENTS; ++i) {
        float a = 2 * PI * i / SEGMENTS;
        SDL_FPoint p = {center.x + radius * SDL_cosf(a),
                        center.y + radius * SDL_sinf(a)};
        geometry_add_vertex(g, p, color);
    }
    for (int i = 0; i < SEGMENTS; ++i) {
        geometry_add_triangle(g, first, first + 1 + i,
                              first + 1 + (i + 1) % SEGMENTS);
    }
}

static void
draw_segment(struct sc_nav_bar_geometry *g, SDL_FPoint p0, SDL_FPoint p1,
             float thickness, SDL_FColor color) {
    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;
    float len = SDL_sqrtf(dx * dx + dy * dy);
    if (len == 0) {
        return;
    }
    // Normal vector, of length thickness/2
    float nx = -dy / len * thickness / 2;
    float ny = dx / len * thickness / 2;

    int first = geometry_reserve(g, 4, 6);
    geometry_add_vertex(g, (SDL_FPoint) {p0.x + nx, p0.y + ny}, color);
    geometry_add_vertex(g, (SDL_FPoint) {p0.x - nx, p0.y - ny}, color);
    geometry_add_vertex(g, (SDL_FPoint) {p1.x - nx, p1.y - ny}, color);
    geometry_add_vertex(g, (SDL_FPoint) {p1.x + nx, p1.y + ny}, color);
    geometry_add_triangle(g, first, first + 1, first + 2);
    geometry_add_triangle(g, first, first + 2, first + 3);
}

static void
draw_polyline(struct sc_nav_bar_geometry *g, const SDL_FPoint *points,
              int count, bool closed, float thickness, SDL_FColor color) {
    int segments = closed ? count : count - 1;
    for (int i = 0; i < segments; ++i) {
        draw_segment(g, points[i], points[(i + 1) % count], thickness, color);
    }
    // Round joints
    for (int i = 0; i < count; ++i) {
        fill_disc(g, points[i], thickness / 2, color);
    }
}

// Angles in radians, clockwise from the positive x axis (y points down)
static void
draw_arc(struct sc_nav_bar_geometry *g, SDL_FPoint center, float radius,
         float from, float to, float thickness, SDL_FColor color) {
    enum { POINTS = 33 };
    SDL_FPoint points[POINTS];
    for (int i = 0; i < POINTS; ++i) {
        float a = from + (to - from) * i / (POINTS - 1);
        points[i].x = center.x + radius * SDL_cosf(a);
        points[i].y = center.y + radius * SDL_sinf(a);
    }
    for (int i = 0; i < POINTS - 1; ++i) {
        draw_segment(g, points[i], points[i + 1], thickness, color);
    }
    // The gaps between consecutive segments are negligible, only round the
    // ends
    fill_disc(g, points[0], thickness / 2, color);
    fill_disc(g, points[POINTS - 1], thickness / 2, color);
}

static void
draw_speaker(struct sc_nav_bar_geometry *g, float cx, float cy, float r,
             float t, SDL_FColor color) {
    SDL_FPoint speaker[] = {
        {cx - r,          cy - 0.35f * r},
        {cx - 0.45f * r,  cy - 0.35f * r},
        {cx + 0.05f * r,  cy - 0.85f * r},
        {cx + 0.05f * r,  cy + 0.85f * r},
        {cx - 0.45f * r,  cy + 0.35f * r},
        {cx - r,          cy + 0.35f * r},
    };
    draw_polyline(g, speaker, 6, true, t, color);
}

static void
draw_icon(struct sc_nav_bar_geometry *g, enum sc_nav_bar_button button,
          float cx, float cy, float r, float t, SDL_FColor color) {
    switch (button) {
        case SC_NAV_BAR_BUTTON_BACK: {
            SDL_FPoint triangle[] = {
                {cx + 0.65f * r, cy - 0.9f * r},
                {cx - 0.85f * r, cy},
                {cx + 0.65f * r, cy + 0.9f * r},
            };
            draw_polyline(g, triangle, 3, true, t, color);
            break;
        }
        case SC_NAV_BAR_BUTTON_HOME:
            draw_arc(g, (SDL_FPoint) {cx, cy}, 0.85f * r, 0,
                     2 * PI, t, color);
            break;
        case SC_NAV_BAR_BUTTON_APP_SWITCH: {
            float h = 0.8f * r;
            SDL_FPoint square[] = {
                {cx - h, cy - h},
                {cx + h, cy - h},
                {cx + h, cy + h},
                {cx - h, cy + h},
            };
            draw_polyline(g, square, 4, true, t, color);
            break;
        }
        case SC_NAV_BAR_BUTTON_VOLUME_DOWN:
        case SC_NAV_BAR_BUTTON_VOLUME_UP: {
            draw_speaker(g, cx - 0.2f * r, cy, r, t, color);
            float sx = cx + 0.65f * r;
            float sh = 0.35f * r;
            draw_segment(g, (SDL_FPoint) {sx - sh, cy},
                         (SDL_FPoint) {sx + sh, cy}, t, color);
            if (button == SC_NAV_BAR_BUTTON_VOLUME_UP) {
                draw_segment(g, (SDL_FPoint) {sx, cy - sh},
                             (SDL_FPoint) {sx, cy + sh}, t, color);
            }
            break;
        }
        case SC_NAV_BAR_BUTTON_ROTATE: {
            // Clockwise circular arrow, open at the top
            float rr = 0.75f * r;
            float from = -PI / 2 + 0.5f;
            float to = from + 2 * PI - 1.f;
            draw_arc(g, (SDL_FPoint) {cx, cy}, rr, from, to, t, color);
            // Arrow head at the end of the arc, along the direction of travel
            float ex = cx + rr * SDL_cosf(to);
            float ey = cy + rr * SDL_sinf(to);
            float nx = SDL_cosf(to); // radial
            float ny = SDL_sinf(to);
            float tx = -ny; // tangent (clockwise, since y points down)
            float ty = nx;
            float s = 0.4f * r;
            fill_triangle(g,
                          (SDL_FPoint) {ex + nx * s, ey + ny * s},
                          (SDL_FPoint) {ex - nx * s, ey - ny * s},
                          (SDL_FPoint) {ex + tx * 1.2f * s, ey + ty * 1.2f * s},
                          color);
            break;
        }
        case SC_NAV_BAR_BUTTON_POWER: {
            // Ring open at the top, and a vertical bar
            draw_arc(g, (SDL_FPoint) {cx, cy + 0.1f * r}, 0.8f * r,
                     -PI / 2 + 0.6f, 3 * PI / 2 - 0.6f, t, color);
            draw_segment(g, (SDL_FPoint) {cx, cy - 0.95f * r},
                         (SDL_FPoint) {cx, cy - 0.05f * r}, t, color);
            break;
        }
        default:
            assert(!"Unexpected button");
    }
}

void
sc_nav_bar_render(struct sc_nav_bar *bar, SDL_Renderer *renderer,
                  float scale) {
    if (bar->rect.w <= 0 || bar->rect.h <= 0) {
        return;
    }

    SDL_FRect rect = {
        .x = bar->rect.x * scale,
        .y = bar->rect.y * scale,
        .w = bar->rect.w * scale,
        .h = bar->rect.h * scale,
    };
    SDL_SetRenderDrawColorFloat(renderer, COLOR_BAR.r, COLOR_BAR.g,
                                COLOR_BAR.b, COLOR_BAR.a);
    SDL_RenderFillRect(renderer, &rect);

    // Large struct, do not allocate it on the stack
    static struct sc_nav_bar_geometry g;
    g.renderer = renderer;
    g.vertex_count = 0;
    g.index_count = 0;

    // The highlights are drawn immediately, the icons (all of the same color)
    // are rendered at the end, over them
    for (int i = 0; i < SC_NAV_BAR_BUTTON_COUNT; ++i) {
        SDL_FRect cell = get_cell_rect(bar, i);
        cell.x *= scale;
        cell.y *= scale;
        cell.w *= scale;
        cell.h *= scale;

        if (i == bar->pressed || i == bar->hovered) {
            SDL_FColor c = i == bar->pressed ? COLOR_PRESSED : COLOR_HOVER;
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
            // Leave a small margin around the highlight
            float m = 3 * scale;
            SDL_FRect hl = {cell.x + m, cell.y + m, cell.w - 2 * m,
                            cell.h - 2 * m};
            SDL_RenderFillRect(renderer, &hl);
        }

        float cx = cell.x + cell.w / 2;
        float cy = cell.y + cell.h / 2;
        draw_icon(&g, i, cx, cy, ICON_RADIUS * scale, STROKE * scale,
                  COLOR_ICON);
    }

    geometry_flush(&g);
}
