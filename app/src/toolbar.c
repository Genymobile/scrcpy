#include "toolbar.h"

#include <math.h>
#include <stdio.h>

#include "icon.h"
#include "util/log.h"

#define SC_TOOLBAR_RADIUS 20.f
#define SC_TOOLBAR_SUPERSAMPLING 4
#define SC_TOOLBAR_ICON_VIEWBOX_SIZE 24.f
#define SC_TOOLBAR_HIGHLIGHT_RADIUS 19.f

struct sc_toolbar_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct sc_icon_bounds {
    float x;
    float y;
    float w;
    float h;
};

struct sc_toolbar_item {
    const char *icon_name;
    struct sc_icon_bounds icon_bounds;
    bool separator_before;
};

static const struct sc_toolbar_color SC_TOOLBAR_BG = {28, 30, 31, 255};
static const struct sc_toolbar_color SC_TOOLBAR_HOVER = {38, 40, 41, 255};
static const struct sc_toolbar_color SC_TOOLBAR_PRESSED = {47, 49, 50, 255};
static const struct sc_toolbar_color SC_TOOLBAR_RECORDING_BG = {
    59, 29, 31, 255,
};
static const struct sc_toolbar_color SC_TOOLBAR_RECORDING_RED = {
    230, 67, 72, 255,
};
static const struct sc_toolbar_color SC_TOOLBAR_BORDER = {77, 80, 81, 255};
static const struct sc_toolbar_color SC_TOOLBAR_DIVIDER = {244, 244, 242, 96};
static const struct sc_toolbar_color SC_TOOLBAR_ICON = {244, 244, 242, 255};
static const struct sc_toolbar_color SC_TOOLBAR_ICON_DISABLED = {
    139, 141, 141, 190,
};

static const struct sc_toolbar_item SC_TOOLBAR_ITEMS[] = {
    [SC_TOOLBAR_ACTION_HOME] = {
        .icon_name = "home",
        .icon_bounds = {2.f, 3.f, 20.f, 17.f},
    },
    [SC_TOOLBAR_ACTION_BACK] = {
        .icon_name = "back",
        .icon_bounds = {4.f, 4.f, 16.f, 16.f},
    },
    [SC_TOOLBAR_ACTION_RECENTS] = {
        // Drawn as a supersampled vector to keep the rounded corners crisp.
        .icon_name = NULL,
    },
    [SC_TOOLBAR_ACTION_SCREENSHOT] = {
        .icon_name = "screenshot",
        .icon_bounds = {3.f, 3.f, 18.f, 18.f},
        .separator_before = true,
    },
    [SC_TOOLBAR_ACTION_RECORD] = {
        .icon_name = "record",
        .icon_bounds = {1.2f, 1.2f, 21.6f, 21.6f},
    },
};

static int
get_icon_asset_size(float density) {
    return density >= 2.5f ? 60 : density >= 1.5f ? 40 : 20;
}

static void
destroy_icon_textures(struct sc_toolbar *toolbar) {
    for (size_t i = 0; i < SC_TOOLBAR_ACTION_COUNT; ++i) {
        if (toolbar->icons[i]) {
            SDL_DestroyTexture(toolbar->icons[i]);
            toolbar->icons[i] = NULL;
        }
    }
}

static void
load_icon_textures(struct sc_toolbar *toolbar, float density) {
    int asset_size = get_icon_asset_size(density);
    if (toolbar->icon_asset_size == asset_size) {
        return;
    }

    destroy_icon_textures(toolbar);
    toolbar->icon_asset_size = asset_size;
    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        const char *icon_name = SC_TOOLBAR_ITEMS[action].icon_name;
        if (!icon_name) {
            continue;
        }
        char filename[64];
        int r = snprintf(filename, sizeof(filename),
                         "toolbar-icons/%s-%d.png",
                         icon_name, asset_size);
        if (r < 0 || (size_t) r >= sizeof(filename)) {
            LOGW("Could not build toolbar icon filename");
            continue;
        }
        SDL_Surface *surface = sc_icon_load(filename);
        if (!surface) {
            LOGW("Could not load toolbar icon: %s", filename);
            continue;
        }
        toolbar->icons[action] =
            SDL_CreateTextureFromSurface(toolbar->renderer, surface);
        sc_icon_destroy(surface);
        if (!toolbar->icons[action]) {
            LOGW("Could not create toolbar icon texture: %s",
                 SDL_GetError());
            continue;
        }
        SDL_SetTextureBlendMode(toolbar->icons[action], SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(toolbar->icons[action], SDL_SCALEMODE_LINEAR);
    }
}

static bool
point_in_rect(float x, float y, const SDL_FRect *rect) {
    return x >= rect->x && x < rect->x + rect->w
        && y >= rect->y && y < rect->y + rect->h;
}

static SDL_FRect
get_icon_rect(const SDL_FRect *button, float scale) {
    float size = SC_TOOLBAR_ICON_SIZE * scale;
    return (SDL_FRect) {
        .x = (button->x + (button->w - SC_TOOLBAR_ICON_SIZE) / 2.f) * scale,
        .y = (button->y + (button->h - SC_TOOLBAR_ICON_SIZE) / 2.f) * scale,
        .w = size,
        .h = size,
    };
}

static bool
point_in_rounded_rect(float x, float y, const SDL_FRect *rect, float radius) {
    if (!point_in_rect(x, y, rect)) {
        return false;
    }

    float left = rect->x + radius;
    float right = rect->x + rect->w - radius;
    float top = rect->y + radius;
    float bottom = rect->y + rect->h - radius;
    if (x >= left && x < right) {
        return true;
    }
    if (y >= top && y < bottom) {
        return true;
    }

    float cx = x < left ? left : right;
    float cy = y < top ? top : bottom;
    float dx = x - cx;
    float dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

void
sc_toolbar_init(struct sc_toolbar *toolbar, bool enabled,
                bool initially_visible) {
    toolbar->enabled = enabled;
    toolbar->user_visible = enabled && initially_visible;
    toolbar->window = NULL;
    toolbar->renderer = NULL;
    for (size_t i = 0; i < SC_TOOLBAR_ACTION_COUNT; ++i) {
        toolbar->icons[i] = NULL;
    }
    toolbar->icon_asset_size = 0;
    toolbar->render_cache = NULL;
    toolbar->render_cache_width = 0;
    toolbar->render_cache_height = 0;
    toolbar->render_cache_navigation_enabled = false;
    toolbar->render_cache_screenshot_enabled = false;
    toolbar->render_cache_record_enabled = false;
    toolbar->render_cache_recording = false;
    toolbar->render_cache_hovered = SC_TOOLBAR_ACTION_NONE;
    toolbar->render_cache_pressed = SC_TOOLBAR_ACTION_NONE;
    toolbar->panel = (SDL_FRect) {0};
    toolbar->hovered = SC_TOOLBAR_ACTION_NONE;
    toolbar->pressed = SC_TOOLBAR_ACTION_NONE;
    for (size_t i = 0; i < SC_TOOLBAR_ACTION_COUNT; ++i) {
        toolbar->buttons[i] = (SDL_FRect) {0};
    }
    sc_toolbar_layout(toolbar);
}

void
sc_toolbar_layout(struct sc_toolbar *toolbar) {
    toolbar->panel = (SDL_FRect) {
        .x = SC_TOOLBAR_SHADOW_MARGIN,
        .y = SC_TOOLBAR_SHADOW_MARGIN,
        .w = SC_TOOLBAR_PANEL_WIDTH,
        .h = SC_TOOLBAR_PANEL_HEIGHT,
    };

    float y = toolbar->panel.y;
    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        if (SC_TOOLBAR_ITEMS[action].separator_before) {
            y += SC_TOOLBAR_SEPARATOR_HEIGHT;
        }
        toolbar->buttons[action] = (SDL_FRect) {
            .x = toolbar->panel.x,
            .y = y,
            .w = toolbar->panel.w,
            .h = SC_TOOLBAR_BUTTON_HEIGHT,
        };
        y += SC_TOOLBAR_BUTTON_HEIGHT;
    }
}

bool
sc_toolbar_create(struct sc_toolbar *toolbar, SDL_Window *parent,
                  bool always_on_top) {
    if (!toolbar->enabled) {
        return true;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return false;
    }

    bool ok =
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              "scrcpy controls");
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                                SC_TOOLBAR_WINDOW_WIDTH);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                                SC_TOOLBAR_WINDOW_HEIGHT);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN,
                                 true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN,
                                 true);
    ok &= SDL_SetBooleanProperty(
        props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_UTILITY_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN,
                                 false);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN,
                                 always_on_top);
    ok &= SDL_SetPointerProperty(props,
                                 SDL_PROP_WINDOW_CREATE_PARENT_POINTER, parent);
    if (!ok) {
        SDL_DestroyProperties(props);
        return false;
    }

    toolbar->window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!toolbar->window) {
        LOGW("Could not create floating toolbar window: %s", SDL_GetError());
        return false;
    }

    toolbar->renderer = SDL_CreateRenderer(toolbar->window, NULL);
    if (!toolbar->renderer) {
        LOGW("Could not create floating toolbar renderer: %s", SDL_GetError());
        SDL_DestroyWindow(toolbar->window);
        toolbar->window = NULL;
        return false;
    }

    SDL_SetRenderDrawBlendMode(toolbar->renderer, SDL_BLENDMODE_BLEND);

    float density = SDL_GetWindowPixelDensity(toolbar->window);
    load_icon_textures(toolbar, density);
    return true;
}

void
sc_toolbar_destroy(struct sc_toolbar *toolbar) {
    destroy_icon_textures(toolbar);
    toolbar->icon_asset_size = 0;
    if (toolbar->render_cache) {
        SDL_DestroyTexture(toolbar->render_cache);
        toolbar->render_cache = NULL;
    }
    if (toolbar->renderer) {
        SDL_DestroyRenderer(toolbar->renderer);
        toolbar->renderer = NULL;
    }
    if (toolbar->window) {
        SDL_DestroyWindow(toolbar->window);
        toolbar->window = NULL;
    }
}

enum sc_toolbar_action
sc_toolbar_hit_test(const struct sc_toolbar *toolbar, float x, float y) {
    if (!sc_toolbar_contains(toolbar, x, y)) {
        return SC_TOOLBAR_ACTION_NONE;
    }

    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        if (point_in_rect(x, y, &toolbar->buttons[action])) {
            return action;
        }
    }
    return SC_TOOLBAR_ACTION_NONE;
}

bool
sc_toolbar_contains(const struct sc_toolbar *toolbar, float x, float y) {
    return toolbar->enabled
        && point_in_rounded_rect(x, y, &toolbar->panel, SC_TOOLBAR_RADIUS);
}

bool
sc_toolbar_is_window(const struct sc_toolbar *toolbar,
                     SDL_WindowID window_id) {
    return toolbar->window && SDL_GetWindowID(toolbar->window) == window_id;
}

void
sc_toolbar_update_position(struct sc_toolbar *toolbar, SDL_Window *parent) {
    if (!toolbar->window) {
        return;
    }

    int parent_x;
    int parent_y;
    int parent_width;
    int parent_height;
    if (!SDL_GetWindowPosition(parent, &parent_x, &parent_y)
            || !SDL_GetWindowSize(parent, &parent_width, &parent_height)) {
        LOGD("Could not read parent window geometry: %s", SDL_GetError());
        return;
    }
    (void) parent_height;
    int panel_x = parent_x + parent_width + SC_TOOLBAR_GAP;
    int panel_y = parent_y + SC_TOOLBAR_TOP_OFFSET;

    SDL_DisplayID display = SDL_GetDisplayForWindow(parent);
    SDL_Rect bounds;
    if (display && SDL_GetDisplayUsableBounds(display, &bounds)) {
        int right = panel_x + SC_TOOLBAR_PANEL_WIDTH;
        if (right > bounds.x + bounds.w) {
            panel_x = parent_x - SC_TOOLBAR_GAP - SC_TOOLBAR_PANEL_WIDTH;
        }
        panel_x = MAX(bounds.x, MIN(panel_x,
                      bounds.x + bounds.w - SC_TOOLBAR_PANEL_WIDTH));
        panel_y = MAX(bounds.y, MIN(panel_y,
                      bounds.y + bounds.h - SC_TOOLBAR_PANEL_HEIGHT));
    }

    if (!SDL_SetWindowPosition(toolbar->window,
                               panel_x - SC_TOOLBAR_SHADOW_MARGIN,
                               panel_y - SC_TOOLBAR_SHADOW_MARGIN)) {
        LOGD("Could not position floating toolbar: %s", SDL_GetError());
    }
}

void
sc_toolbar_show(struct sc_toolbar *toolbar) {
    if (!toolbar->window) {
        return;
    }
    if (SDL_GetWindowFlags(toolbar->window) & SDL_WINDOW_HIDDEN) {
        if (!SDL_ShowWindow(toolbar->window)) {
            LOGW("Could not show floating toolbar: %s", SDL_GetError());
        }
    }
}

void
sc_toolbar_hide(struct sc_toolbar *toolbar) {
    if (!toolbar->window) {
        return;
    }
    if (!(SDL_GetWindowFlags(toolbar->window) & SDL_WINDOW_HIDDEN)) {
        if (!SDL_HideWindow(toolbar->window)) {
            LOGW("Could not hide floating toolbar: %s", SDL_GetError());
        }
    }
}

static void
set_color(SDL_Renderer *renderer, struct sc_toolbar_color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static float
rounded_rect_distance(float x, float y, const SDL_FRect *rect,
                      float radius) {
    float cx = rect->x + rect->w / 2.f;
    float cy = rect->y + rect->h / 2.f;
    float bx = rect->w / 2.f - radius;
    float by = rect->h / 2.f - radius;
    float qx = fabsf(x - cx) - bx;
    float qy = fabsf(y - cy) - by;
    float outside = hypotf(MAX(qx, 0.f), MAX(qy, 0.f));
    float inside = MIN(MAX(qx, qy), 0.f);
    return outside + inside - radius;
}

static float
coverage(float distance) {
    return MAX(0.f, MIN(1.f, .5f - distance));
}

static void
composite_color(float *r, float *g, float *b, float *a,
                struct sc_toolbar_color color, float coverage_value) {
    float src_a = color.a / 255.f * coverage_value;
    float dst_a = *a;
    float out_a = src_a + dst_a * (1.f - src_a);
    if (out_a > 0.f) {
        *r = (color.r / 255.f * src_a + *r * dst_a * (1.f - src_a))
           / out_a;
        *g = (color.g / 255.f * src_a + *g * dst_a * (1.f - src_a))
           / out_a;
        *b = (color.b / 255.f * src_a + *b * dst_a * (1.f - src_a))
           / out_a;
    }
    *a = out_a;
}

static SDL_Texture *
create_panel_texture(SDL_Renderer *renderer, float scale) {
    int width = (int) lroundf(SC_TOOLBAR_WINDOW_WIDTH * scale);
    int height = (int) lroundf(SC_TOOLBAR_WINDOW_HEIGHT * scale);
    SDL_Surface *surface = SDL_CreateSurface(width, height,
                                             SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return NULL;
    }

    const SDL_PixelFormatDetails *details =
        SDL_GetPixelFormatDetails(surface->format);
    SDL_FRect outer = {
        .x = SC_TOOLBAR_SHADOW_MARGIN * scale,
        .y = SC_TOOLBAR_SHADOW_MARGIN * scale,
        .w = SC_TOOLBAR_PANEL_WIDTH * scale,
        .h = SC_TOOLBAR_PANEL_HEIGHT * scale,
    };
    SDL_FRect inner = {
        .x = outer.x + scale,
        .y = outer.y + scale,
        .w = outer.w - 2.f * scale,
        .h = outer.h - 2.f * scale,
    };
    SDL_FRect shadow = outer;
    shadow.x += scale;
    shadow.y += 2.5f * scale;
    float outer_radius = SC_TOOLBAR_RADIUS * scale;
    float inner_radius = (SC_TOOLBAR_RADIUS - 1.f) * scale;
    float blur = 3.f * scale;

    for (int y = 0; y < height; ++y) {
        Uint32 *row = (Uint32 *) ((Uint8 *) surface->pixels
                                 + y * surface->pitch);
        for (int x = 0; x < width; ++x) {
            float px = x + .5f;
            float py = y + .5f;
            float r = 0.f;
            float g = 0.f;
            float b = 0.f;
            float a = 0.f;

            float shadow_distance = rounded_rect_distance(px, py, &shadow,
                                                          outer_radius);
            float shadow_coverage = shadow_distance <= 0.f
                                  ? 1.f
                                  : MAX(0.f, 1.f - shadow_distance / blur);
            composite_color(&r, &g, &b, &a,
                            (struct sc_toolbar_color) {0, 0, 0, 62},
                            shadow_coverage);

            float outer_coverage = coverage(rounded_rect_distance(
                px, py, &outer, outer_radius));
            composite_color(&r, &g, &b, &a, SC_TOOLBAR_BORDER,
                            outer_coverage);

            float inner_coverage = coverage(rounded_rect_distance(
                px, py, &inner, inner_radius));
            composite_color(&r, &g, &b, &a, SC_TOOLBAR_BG,
                            inner_coverage);

            row[x] = SDL_MapRGBA(details, NULL,
                                 (Uint8) lroundf(r * 255.f),
                                 (Uint8) lroundf(g * 255.f),
                                 (Uint8) lroundf(b * 255.f),
                                 (Uint8) lroundf(a * 255.f));
        }
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    }
    return texture;
}

static void
fill_rounded_rect(SDL_Renderer *renderer, SDL_FRect rect, float radius) {
    int y_begin = (int) ceilf(rect.y);
    int y_end = (int) floorf(rect.y + rect.h);
    float top_center = rect.y + radius;
    float bottom_center = rect.y + rect.h - radius;
    for (int y = y_begin; y < y_end; ++y) {
        float py = y + .5f;
        float inset = 0.f;
        if (py < top_center) {
            float dy = top_center - py;
            inset = radius - sqrtf(MAX(0.f, radius * radius - dy * dy));
        } else if (py > bottom_center) {
            float dy = py - bottom_center;
            inset = radius - sqrtf(MAX(0.f, radius * radius - dy * dy));
        }
        SDL_RenderLine(renderer, ceilf(rect.x + inset), y + .5f,
                       floorf(rect.x + rect.w - inset), y + .5f);
    }
}

static void
fill_circle(SDL_Renderer *renderer, float cx, float cy, float radius) {
    int y_begin = (int) ceilf(cy - radius);
    int y_end = (int) floorf(cy + radius);
    for (int y = y_begin; y <= y_end; ++y) {
        float dy = y + .5f - cy;
        float dx = sqrtf(MAX(0.f, radius * radius - dy * dy));
        SDL_RenderLine(renderer, cx - dx, y + .5f, cx + dx, y + .5f);
    }
}

static void
draw_thick_line(SDL_Renderer *renderer, float x1, float y1, float x2,
                float y2, float thickness) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    if (!length) {
        return;
    }
    float nx = -dy / length;
    float ny = dx / length;
    int count = MAX(1, (int) ceilf(thickness));
    float start = -(count - 1) / 2.f;
    for (int i = 0; i < count; ++i) {
        float offset = start + i;
        SDL_RenderLine(renderer, x1 + nx * offset, y1 + ny * offset,
                       x2 + nx * offset, y2 + ny * offset);
    }
}

static SDL_FColor
to_fcolor(struct sc_toolbar_color color) {
    return (SDL_FColor) {
        color.r / 255.f,
        color.g / 255.f,
        color.b / 255.f,
        color.a / 255.f,
    };
}

static void
fill_triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b,
              SDL_FPoint c, struct sc_toolbar_color color) {
    SDL_FColor vertex_color = to_fcolor(color);
    SDL_Vertex vertices[] = {
        { .position = a, .color = vertex_color },
        { .position = b, .color = vertex_color },
        { .position = c, .color = vertex_color },
    };
    SDL_RenderGeometry(renderer, NULL, vertices, ARRAY_LEN(vertices), NULL, 0);
}

static struct sc_toolbar_color
get_segment_color(const struct sc_toolbar *toolbar,
                  enum sc_toolbar_action action) {
    if (toolbar->pressed == action) {
        return SC_TOOLBAR_PRESSED;
    }
    if (toolbar->hovered == action) {
        return SC_TOOLBAR_HOVER;
    }
    return SC_TOOLBAR_BG;
}

static void
draw_home(SDL_Renderer *renderer, const SDL_FRect *button, float scale,
          struct sc_toolbar_color icon, struct sc_toolbar_color segment) {
    float cx = (button->x + button->w / 2.f) * scale;
    float cy = (button->y + button->h / 2.f) * scale;
    float half_w = 8.f * scale;
    float roof_y = cy - 8.f * scale;
    float shoulder_y = cy - 1.f * scale;
    fill_triangle(renderer, (SDL_FPoint) {cx, roof_y},
                  (SDL_FPoint) {cx - half_w, shoulder_y},
                  (SDL_FPoint) {cx + half_w, shoulder_y}, icon);
    set_color(renderer, icon);
    SDL_FRect body = {
        .x = cx - 6.f * scale,
        .y = shoulder_y - 1.f * scale,
        .w = 12.f * scale,
        .h = 10.f * scale,
    };
    SDL_RenderFillRect(renderer, &body);
    set_color(renderer, segment);
    SDL_FRect door = {
        .x = cx - 1.75f * scale,
        .y = cy + 3.f * scale,
        .w = 3.5f * scale,
        .h = 5.f * scale,
    };
    SDL_RenderFillRect(renderer, &door);
}

static void
draw_back(SDL_Renderer *renderer, const SDL_FRect *button, float scale,
          struct sc_toolbar_color icon) {
    float cx = (button->x + button->w / 2.f) * scale;
    float cy = (button->y + button->h / 2.f) * scale;
    float t = 1.8f * scale;
    set_color(renderer, icon);

    SDL_FPoint previous = {cx - 3.5f * scale, cy - 2.5f * scale};
    for (int i = 1; i <= 20; ++i) {
        float u = i / 20.f;
        float inv = 1.f - u;
        SDL_FPoint point = {
            .x = inv * inv * (cx - 3.5f * scale)
               + 2.f * inv * u * (cx + 7.5f * scale)
               + u * u * (cx + 7.5f * scale),
            .y = inv * inv * (cy - 2.5f * scale)
               + 2.f * inv * u * (cy - 2.5f * scale)
               + u * u * (cy + 6.5f * scale),
        };
        draw_thick_line(renderer, previous.x, previous.y, point.x, point.y, t);
        previous = point;
    }
    draw_thick_line(renderer, cx - 7.f * scale, cy - 2.5f * scale,
                    cx - 2.5f * scale, cy - 2.5f * scale, t);
    fill_triangle(renderer,
                  (SDL_FPoint) {cx - 8.5f * scale, cy - 2.5f * scale},
                  (SDL_FPoint) {cx - 2.5f * scale, cy - 8.f * scale},
                  (SDL_FPoint) {cx - 2.5f * scale, cy + 3.f * scale}, icon);
}

static void
draw_screenshot(SDL_Renderer *renderer, const SDL_FRect *button, float scale,
                struct sc_toolbar_color icon) {
    float cx = (button->x + button->w / 2.f) * scale;
    float cy = (button->y + button->h / 2.f) * scale;
    float left = cx - 8.f * scale;
    float right = cx + 8.f * scale;
    float top = cy - 8.f * scale;
    float bottom = cy + 8.f * scale;
    float leg = 4.f * scale;
    float t = 1.6f * scale;
    set_color(renderer, icon);

    draw_thick_line(renderer, left, top + leg, left, top, t);
    draw_thick_line(renderer, left, top, left + leg, top, t);
    draw_thick_line(renderer, right - leg, top, right, top, t);
    draw_thick_line(renderer, right, top, right, top + leg, t);
    draw_thick_line(renderer, left, bottom - leg, left, bottom, t);
    draw_thick_line(renderer, left, bottom, left + leg, bottom, t);
    draw_thick_line(renderer, right - leg, bottom, right, bottom, t);
    draw_thick_line(renderer, right, bottom - leg, right, bottom, t);
    fill_circle(renderer, cx, cy, 3.1f * scale);
}

static void
draw_recents(SDL_Renderer *renderer, const SDL_FRect *button, float scale,
             struct sc_toolbar_color icon,
             struct sc_toolbar_color segment) {
    SDL_FRect outer = get_icon_rect(button, scale);
    float cx = outer.x + outer.w / 2.f;
    float cy = outer.y + outer.h / 2.f;
    set_color(renderer, icon);
    fill_rounded_rect(renderer, outer, 3.f * scale);

    SDL_FRect inner = {
        .x = cx - 5.75f * scale,
        .y = cy - 5.75f * scale,
        .w = 11.5f * scale,
        .h = 11.5f * scale,
    };
    set_color(renderer, segment);
    fill_rounded_rect(renderer, inner, 1.5f * scale);
}

static bool
action_enabled(enum sc_toolbar_action action,
               const struct sc_toolbar_view_state *state) {
    if (action == SC_TOOLBAR_ACTION_SCREENSHOT) {
        return state->screenshot_enabled;
    }
    if (action == SC_TOOLBAR_ACTION_RECORD) {
        return state->record_enabled;
    }
    return state->navigation_enabled;
}

static bool
render_icon_texture(struct sc_toolbar *toolbar,
                    enum sc_toolbar_action action, float scale,
                    struct sc_toolbar_color color) {
    SDL_Texture *texture = toolbar->icons[action];
    if (!texture) {
        return false;
    }

    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    float texture_width;
    float texture_height;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height)) {
        return false;
    }

    const struct sc_icon_bounds *bounds =
        &SC_TOOLBAR_ITEMS[action].icon_bounds;
    SDL_FRect src = {
        .x = bounds->x / SC_TOOLBAR_ICON_VIEWBOX_SIZE * texture_width,
        .y = bounds->y / SC_TOOLBAR_ICON_VIEWBOX_SIZE * texture_height,
        .w = bounds->w / SC_TOOLBAR_ICON_VIEWBOX_SIZE * texture_width,
        .h = bounds->h / SC_TOOLBAR_ICON_VIEWBOX_SIZE * texture_height,
    };
    const SDL_FRect *button = &toolbar->buttons[action];
    SDL_FRect dst = get_icon_rect(button, scale);
    return SDL_RenderTexture(toolbar->renderer, texture, &src, &dst);
}

static void
render_icon_textures(struct sc_toolbar *toolbar, float scale,
                     const struct sc_toolbar_view_state *state) {
    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        bool enabled = action_enabled(action, state);
        struct sc_toolbar_color icon = enabled
                                     ? SC_TOOLBAR_ICON
                                     : SC_TOOLBAR_ICON_DISABLED;
        if (toolbar->icons[action]
                && !render_icon_texture(toolbar, action, scale, icon)) {
            LOGW("Could not render floating toolbar icon: %s",
                 SDL_GetError());
        }
        if (action == SC_TOOLBAR_ACTION_RECORD && state->recording) {
            const SDL_FRect *button = &toolbar->buttons[action];
            set_color(toolbar->renderer, SC_TOOLBAR_RECORDING_RED);
            fill_circle(toolbar->renderer,
                        (button->x + button->w / 2.f) * scale,
                        (button->y + button->h / 2.f) * scale,
                        5.f * scale);
        }
    }
}

void
sc_toolbar_render(struct sc_toolbar *toolbar,
                  const struct sc_toolbar_view_state *state) {
    if (!toolbar->renderer || !toolbar->window) {
        return;
    }

    SDL_Renderer *renderer = toolbar->renderer;
    float scale = SDL_GetWindowPixelDensity(toolbar->window);
    if (!scale) {
        scale = 1.f;
    }
    load_icon_textures(toolbar, scale);

    int output_width;
    int output_height;
    bool has_output_size =
        SDL_GetRenderOutputSize(renderer, &output_width, &output_height);
    bool cache_valid = has_output_size && toolbar->render_cache
                    && toolbar->render_cache_width == output_width
                    && toolbar->render_cache_height == output_height
                    && toolbar->render_cache_navigation_enabled
                           == state->navigation_enabled
                    && toolbar->render_cache_screenshot_enabled
                           == state->screenshot_enabled
                    && toolbar->render_cache_record_enabled
                           == state->record_enabled
                    && toolbar->render_cache_recording == state->recording
                    && toolbar->render_cache_hovered == toolbar->hovered
                    && toolbar->render_cache_pressed == toolbar->pressed;
    if (cache_valid) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(renderer, toolbar->render_cache, NULL, NULL);
        render_icon_textures(toolbar, scale, state);
        if (!SDL_RenderPresent(renderer)) {
            LOGW("Could not present floating toolbar: %s", SDL_GetError());
        }
        return;
    }

    if (toolbar->render_cache) {
        SDL_DestroyTexture(toolbar->render_cache);
        toolbar->render_cache = NULL;
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    SDL_Texture *supersampled = NULL;
    float draw_scale = scale;
    if (has_output_size) {
        supersampled = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
            output_width * SC_TOOLBAR_SUPERSAMPLING,
            output_height * SC_TOOLBAR_SUPERSAMPLING);
        if (supersampled
                && SDL_SetRenderTarget(renderer, supersampled)) {
            SDL_SetTextureBlendMode(supersampled, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(supersampled, SDL_SCALEMODE_LINEAR);
            draw_scale *= SC_TOOLBAR_SUPERSAMPLING;
        } else {
            if (supersampled) {
                SDL_DestroyTexture(supersampled);
                supersampled = NULL;
            }
            SDL_SetRenderTarget(renderer, previous_target);
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Texture *panel_texture = create_panel_texture(renderer, draw_scale);
    if (panel_texture) {
        SDL_RenderTexture(renderer, panel_texture, NULL, NULL);
        SDL_DestroyTexture(panel_texture);
    }

    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        bool recording = action == SC_TOOLBAR_ACTION_RECORD
                      && state->recording;
        bool active = recording || toolbar->hovered == action
                   || toolbar->pressed == action;
        if (active) {
            const SDL_FRect *button = &toolbar->buttons[action];
            set_color(renderer, recording ? SC_TOOLBAR_RECORDING_BG
                                          : get_segment_color(toolbar,
                                                              action));
            fill_circle(renderer,
                        (button->x + button->w / 2.f) * draw_scale,
                        (button->y + button->h / 2.f) * draw_scale,
                        SC_TOOLBAR_HIGHLIGHT_RADIUS * draw_scale);
        }
    }

    set_color(renderer, SC_TOOLBAR_DIVIDER);
    SDL_FRect divider = {
        .x = (toolbar->panel.x + 12.f) * draw_scale,
        .y = (toolbar->panel.y + 3 * SC_TOOLBAR_BUTTON_HEIGHT
            + SC_TOOLBAR_SEPARATOR_HEIGHT / 2.f - .5f) * draw_scale,
        .w = (toolbar->panel.w - 24.f) * draw_scale,
        .h = draw_scale,
    };
    fill_rounded_rect(renderer, divider, .5f * draw_scale);

    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        if (toolbar->icons[action]) {
            continue;
        }
        bool enabled = action_enabled(action, state);
        struct sc_toolbar_color icon = enabled
                                     ? SC_TOOLBAR_ICON
                                     : SC_TOOLBAR_ICON_DISABLED;
        struct sc_toolbar_color segment =
            get_segment_color(toolbar, action);
        switch (action) {
            case SC_TOOLBAR_ACTION_HOME:
                draw_home(renderer, &toolbar->buttons[action], draw_scale,
                          icon, segment);
                break;
            case SC_TOOLBAR_ACTION_BACK:
                draw_back(renderer, &toolbar->buttons[action], draw_scale,
                          icon);
                break;
            case SC_TOOLBAR_ACTION_RECENTS:
                draw_recents(renderer, &toolbar->buttons[action], draw_scale,
                             icon, segment);
                break;
            case SC_TOOLBAR_ACTION_SCREENSHOT:
                draw_screenshot(renderer, &toolbar->buttons[action],
                                draw_scale, icon);
                break;
            case SC_TOOLBAR_ACTION_RECORD: {
                float cx = (toolbar->buttons[action].x
                          + toolbar->buttons[action].w / 2.f) * draw_scale;
                float cy = (toolbar->buttons[action].y
                          + toolbar->buttons[action].h / 2.f) * draw_scale;
                set_color(renderer, icon);
                fill_circle(renderer, cx, cy, 8.f * draw_scale);
                set_color(renderer, get_segment_color(toolbar, action));
                fill_circle(renderer, cx, cy, 6.8f * draw_scale);
                set_color(renderer,
                          state->recording
                              ? SC_TOOLBAR_RECORDING_RED : icon);
                fill_circle(renderer, cx, cy,
                            (state->recording ? 5.f : 3.5f) * draw_scale);
                break;
            }
            default:
                break;
        }
    }

    if (supersampled) {
        SDL_SetRenderTarget(renderer, previous_target);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if (!SDL_RenderTexture(renderer, supersampled, NULL, NULL)) {
            LOGW("Could not downsample floating toolbar: %s",
                 SDL_GetError());
        }
        toolbar->render_cache = supersampled;
        toolbar->render_cache_width = output_width;
        toolbar->render_cache_height = output_height;
        toolbar->render_cache_navigation_enabled = state->navigation_enabled;
        toolbar->render_cache_screenshot_enabled = state->screenshot_enabled;
        toolbar->render_cache_record_enabled = state->record_enabled;
        toolbar->render_cache_recording = state->recording;
        toolbar->render_cache_hovered = toolbar->hovered;
        toolbar->render_cache_pressed = toolbar->pressed;
    }

    render_icon_textures(toolbar, scale, state);

    if (!SDL_RenderPresent(renderer)) {
        LOGW("Could not present floating toolbar: %s", SDL_GetError());
    }
}
