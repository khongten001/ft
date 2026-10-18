/*
 * floating_nav_bar.c — Modern Floating Navigation Bar & Capsule HUD Example
 * =========================================================================
 * Demonstrates how to create floating overlay widgets in Floria Toolkit:
 *
 * 1. Without Abstraction (Manual Top Capsule):
 *    - Uses a standard micro-flexbox hbox attached directly to the window over a canvas.
 *    - Styled with raw CSS: border-radius: 9999px, backdrop-filter: blur(), and box-shadow.
 *    - Anchored manually via ft_window_on_resize callback.
 *    - Illustrates the foundational overlay mechanics useful for building lightweight
 *      HTML doc overlays, popup inspectors, or QML-like dynamic UI layers.
 *
 * 2. With Abstraction (Built-in Bottom Floating Bar):
 *    - Uses ft_floating_bar_create(win, FT_ANCHOR_BOTTOM_CENTER, w, h).
 *    - Built-in automatic centering, margin preservation, and acrylic glass rendering.
 *
 * Both bars float seamlessly over an interactive canvas with zero click interference!
 */

#include "ft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static FtWidget s_manual_top_bar = NULL;
static FtWidget s_bottom_bar     = NULL;
static FtWidget s_drawing_area   = NULL;
static FtWidget s_status_text    = NULL;

static int s_active_tab_top = 0;
static int s_active_tab_bot = 0;
static FtWidget s_top_tab_btns[3];
static FtWidget s_bot_tab_btns[3];

/* Interactive Canvas State */
static double s_click_points[32][2];
static int    s_click_count = 0;

static void update_tab_styles(void)
{
    int is_dark = ft_theme_get_dark_mode();

    /* Update Top Tabs (Manual Bar) */
    for (int i = 0; i < 3; i++) {
        if (i == s_active_tab_top) {
            ft_widget_set_style(s_top_tab_btns[i],
                "background-color: #0284c7; color: #ffffff; font-weight: bold; "
                "border-radius: 9999px; border: none;");
        } else {
            if (is_dark) {
                ft_widget_set_style(s_top_tab_btns[i],
                    "background-color: rgba(255, 255, 255, 0.08); color: #cbd5e1; "
                    "border-radius: 9999px; border: 1px solid rgba(255, 255, 255, 0.12);");
            } else {
                ft_widget_set_style(s_top_tab_btns[i],
                    "background-color: rgba(0, 0, 0, 0.04); color: #334155; "
                    "border-radius: 9999px; border: 1px solid rgba(0, 0, 0, 0.08);");
            }
        }
    }

    /* Update Bottom Tabs (Abstracted Bar) */
    for (int i = 0; i < 3; i++) {
        if (i == s_active_tab_bot) {
            ft_widget_set_style(s_bot_tab_btns[i],
                "background-color: #3b82f6; color: #ffffff; font-weight: bold; "
                "border-radius: 9999px; border: none;");
        } else {
            if (is_dark) {
                ft_widget_set_style(s_bot_tab_btns[i],
                    "background-color: rgba(255, 255, 255, 0.08); color: #cbd5e1; "
                    "border-radius: 9999px; border: 1px solid rgba(255, 255, 255, 0.12);");
            } else {
                ft_widget_set_style(s_bot_tab_btns[i],
                    "background-color: rgba(0, 0, 0, 0.04); color: #334155; "
                    "border-radius: 9999px; border: 1px solid rgba(0, 0, 0, 0.08);");
            }
        }
    }
}

static void on_top_tab_clicked(FtWidget btn, void* user_data)
{
    long idx = (long)user_data;
    s_active_tab_top = (int)idx;
    update_tab_styles();

    const char* names[] = { "Overview", "Documentation", "QML Engine" };
    char buf[128];
    snprintf(buf, sizeof(buf), "Active: Top -> %s", names[idx]);
    ft_text_set_text(s_status_text, buf);
}

static void on_bot_tab_clicked(FtWidget btn, void* user_data)
{
    long idx = (long)user_data;
    s_active_tab_bot = (int)idx;
    update_tab_styles();

    const char* names[] = { "Canvas View", "Component Tree", "Inspector" };
    char buf[128];
    snprintf(buf, sizeof(buf), "Active: Bottom -> %s", names[idx]);
    ft_text_set_text(s_status_text, buf);
}

static void on_dark_mode_toggle(FtWidget sw, int32_t checked, void* user_data)
{
    ft_theme_set_dark_mode(checked);
    update_tab_styles();
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
}

/* ───────────────────────────────────────────────────────────────────────── */
/* 1. Manual Overlay Window Resize Handler (Without Abstraction)             */
/* ───────────────────────────────────────────────────────────────────────── */
static void on_window_resize(FtWidget window, int32_t w, int32_t h, void* user_data)
{
    /* Without abstraction, the application manually positions its overlay widgets */
    if (s_manual_top_bar) {
        int top_w = 460;
        int top_h = 44;
        int top_x = (w - top_w) / 2;
        int top_y = 18;
        ft_widget_set_bounds(s_manual_top_bar, top_x, top_y, top_w, top_h);
        ft_flexbox_update_layout(s_manual_top_bar);
    }

    /* Note: s_bottom_bar is built with ft_floating_bar_create and auto-recenters
     * automatically without requiring any manual math here! */
}

/* ───────────────────────────────────────────────────────────────────────── */
/* Background Canvas: Interactive visual workspace underneath floating bars   */
/* ───────────────────────────────────────────────────────────────────────── */
static void on_paint_canvas(FtWidget area, FtCanvas canvas, void* user_data)
{
    int32_t w = ft_widget_get_width(area);
    int32_t h = ft_widget_get_height(area);
    int is_dark = ft_theme_get_dark_mode();

    /* Background Fill */
    if (is_dark) {
        ft_canvas_draw_rect(canvas, 0, 0, w, h, 0.08, 0.10, 0.15, 1.0);
    } else {
        ft_canvas_draw_rect(canvas, 0, 0, w, h, 0.94, 0.96, 0.98, 1.0);
    }

    /* Subtle grid pattern */
    double grid_step = 40.0;
    double grid_color = is_dark ? 0.20 : 0.85;
    double grid_alpha = is_dark ? 0.25 : 0.45;
    for (double x = 0; x < w; x += grid_step) {
        ft_canvas_draw_line(canvas, x, 0, x, h, 1.0, grid_color, grid_color, grid_color, grid_alpha);
    }
    for (double y = 0; y < h; y += grid_step) {
        ft_canvas_draw_line(canvas, 0, y, w, y, 1.0, grid_color, grid_color, grid_color, grid_alpha);
    }

    /* Colorful interactive vector cards scattered in the workspace */
    struct CardDef {
        double x, y, cw, ch, r, g, b;
        const char* title;
        const char* subtitle;
    } cards[] = {
        { 60,  90, 260, 160, 0.25, 0.55, 0.95, "Vector Graphics", "AggPas 2D Rendering Engine" },
        { 350, 120, 260, 160, 0.85, 0.35, 0.55, "Frosted Acrylic", "Real-time backdrop-filter: blur()" },
        { 640, 90, 260, 160, 0.15, 0.70, 0.50, "Zero-Bloat UI", "Fast native dotted Pascal + C API" },
        { 180, 310, 280, 170, 0.95, 0.60, 0.20, "QML-Style Trees", "Dynamic declarative overlay UI" },
        { 500, 310, 280, 170, 0.60, 0.35, 0.90, "Non-Blocking Hits", "Events pass through outside bounds" },
    };

    for (int i = 0; i < 5; i++) {
        /* Card drop shadow */
        ft_canvas_draw_rounded_rect(canvas, cards[i].x + 4, cards[i].y + 5,
                                    cards[i].cw, cards[i].ch, 14.0,
                                    0.0, 0.0, 0.0, is_dark ? 0.40 : 0.12);
        /* Card body */
        double body_r = is_dark ? 0.14 : 1.0;
        double body_g = is_dark ? 0.17 : 1.0;
        double body_b = is_dark ? 0.23 : 1.0;
        ft_canvas_draw_rounded_rect(canvas, cards[i].x, cards[i].y,
                                    cards[i].cw, cards[i].ch, 14.0,
                                    body_r, body_g, body_b, 1.0);

        /* Accent top bar - clipped to the card's rounded boundary */
        ft_canvas_push_clip_rounded_rect(canvas, cards[i].x, cards[i].y, cards[i].cw, cards[i].ch, 14.0);
        ft_canvas_draw_rect(canvas, (int32_t)cards[i].x, (int32_t)cards[i].y,
                            (int32_t)cards[i].cw, 8,
                            cards[i].r, cards[i].g, cards[i].b, 1.0);
        ft_canvas_pop_clip_rounded_rect(canvas);

        /* Card outline */
        ft_canvas_draw_rounded_rect_outline(canvas, cards[i].x, cards[i].y,
                                            cards[i].cw, cards[i].ch, 14.0, 1.0,
                                            is_dark ? 0.3 : 0.85,
                                            is_dark ? 0.35 : 0.88,
                                            is_dark ? 0.45 : 0.92, 0.8);

        /* Card text */
        ft_canvas_draw_text(canvas, cards[i].x + 18, cards[i].y + 42, cards[i].title, NULL,
                            is_dark ? 0.92 : 0.12, is_dark ? 0.95 : 0.15, is_dark ? 0.98 : 0.20);
        ft_canvas_draw_text(canvas, cards[i].x + 18, cards[i].y + 70, cards[i].subtitle, NULL,
                            0.50, 0.58, 0.68);
    }

    /* Draw user click markers to prove clicks register on canvas! */
    for (int i = 0; i < s_click_count; i++) {
        ft_canvas_draw_circle(canvas, s_click_points[i][0], s_click_points[i][1], 12.0,
                              0.02, 0.52, 0.78, 0.25);
        ft_canvas_draw_circle(canvas, s_click_points[i][0], s_click_points[i][1], 5.0,
                              0.02, 0.52, 0.78, 0.90);
    }
}

static void on_mouse_down_canvas(FtWidget area, int32_t x, int32_t y, int32_t button, void* user_data)
{
    if (button == 1) {
        if (s_click_count < 32) {
            s_click_points[s_click_count][0] = (double)x;
            s_click_points[s_click_count][1] = (double)y;
            s_click_count++;
        } else {
            s_click_count = 0;
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "Canvas Clicked at (%d, %d) — Background Intercepted Event!", x, y);
        ft_text_set_text(s_status_text, buf);
        ft_widget_invalidate(area);
    }
}

int main(int argc, char** argv)
{
    printf("[Floating NavBar Example] Initializing Floria Toolkit...\n");

    /* 1. Create main application window (960x640) */
    FtWidget win = ft_window_create(960, 640, "Floria Toolkit — Floating Overlay Navigation Showcase");
    ft_window_on_resize(win, on_window_resize, NULL);

    /* 2. Create the Root Layout (fills the window underneath floating overlays) */
    FtWidget root = ft_vbox_create(win, 0, 0, 960, 640);
    ft_window_set_layout(win, root);

    /* Canvas Drawing Area inside root layout (expands to fill the window) */
    s_drawing_area = ft_drawingarea_create(root, 0, 0, 960, 640);
    ft_widget_set_flex_grow(s_drawing_area, 1.0);
    ft_drawingarea_on_paint(s_drawing_area, on_paint_canvas, NULL);
    ft_drawingarea_on_mouse_down(s_drawing_area, on_mouse_down_canvas, NULL);

    /* ───────────────────────────────────────────────────────────────────── */
    /* OPTION 1: Top Floating Capsule (WITHOUT ABSTRACTION)                  */
    /* ───────────────────────────────────────────────────────────────────── */
    /* Added directly to `win` AFTER root layout.
     * Painter's algorithm renders it strictly ON TOP of the canvas.
     * Hit-testing checks it FIRST in reverse order. */
    s_manual_top_bar = ft_hbox_create(win, (960 - 460) / 2, 18, 460, 44);
    ft_container_set_corner_radius(s_manual_top_bar, 9999.0);
    ft_container_set_backdrop_blur(s_manual_top_bar, 16.0);
    ft_container_set_padding(s_manual_top_bar, 14.0, 5.0);
    ft_flexbox_set_gap(s_manual_top_bar, 8.0);
    ft_flexbox_set_align_items(s_manual_top_bar, FT_ALIGN_CENTER);

    /* Raw CSS styling: translucent background, subtle border, elevation drop shadow */
    ft_widget_set_style(s_manual_top_bar,
        "background-color: rgba(255, 255, 255, 0.84); "
        "border: 1px solid rgba(255, 255, 255, 0.50); "
        "box-shadow: 1;");

    /* Top Capsule Badge */
    FtWidget badge = ft_text_create(s_manual_top_bar, 0, 0, 75, 26, "RAW HUD");
    ft_widget_set_style(badge, "font-size: 11px; font-weight: bold; color: #0284c7; text-align: center;");

    /* Top Tabs */
    const char* top_tab_labels[] = { "Overview", "Docs", "QML View" };
    for (long i = 0; i < 3; i++) {
        s_top_tab_btns[i] = ft_button_create(s_manual_top_bar, 0, 0, 85, 30, top_tab_labels[i]);
        ft_button_on_click(s_top_tab_btns[i], on_top_tab_clicked, (void*)i);
    }

    ft_spacer_create(s_manual_top_bar);

    /* Mini Close / Info icon button */
    FtWidget btn_info = ft_button_create(s_manual_top_bar, 0, 0, 30, 30, "?");
    ft_widget_set_style(btn_info, "border-radius: 9999px; font-weight: bold;");

    /* ───────────────────────────────────────────────────────────────────── */
    /* OPTION 2: Bottom Floating Bar (WITH FIRST-CLASS ABSTRACTION)          */
    /* ───────────────────────────────────────────────────────────────────── */
    /* Created with ft_floating_bar_create:
     * - Auto-anchors to Bottom Center (FT_ANCHOR_BOTTOM_CENTER)
     * - Auto-recenters on window resize with zero boilerplate math
     * - Default frosted glass acrylic blur and drop shadow */
    s_bottom_bar = ft_floating_bar_create(win, FT_ANCHOR_BOTTOM_CENTER, 390, 50);
    ft_floating_bar_set_margins(s_bottom_bar, 16.0, 20.0);
    ft_container_set_padding(s_bottom_bar, 14.0, 6.0);
    ft_flexbox_set_gap(s_bottom_bar, 8.0);

    /* Bottom Navigation Tabs */
    const char* bot_tab_labels[] = { "Canvas", "Layers", "Inspector" };
    for (long i = 0; i < 3; i++) {
        s_bot_tab_btns[i] = ft_button_create(s_bottom_bar, 0, 0, 80, 32, bot_tab_labels[i]);
        ft_button_on_click(s_bot_tab_btns[i], on_bot_tab_clicked, (void*)i);
    }

    ft_spacer_create(s_bottom_bar);

    /* Dark Mode Switch inside the Floating Bar */
    FtWidget dark_lbl = ft_text_create(s_bottom_bar, 0, 0, 36, 24, "Dark");
    ft_widget_set_style(dark_lbl, "font-size: 11px; color: #64748b;");
    FtWidget dark_sw = ft_switch_create(s_bottom_bar, 0, 0, 42, 22, "");
    ft_switch_set_checked(dark_sw, ft_theme_get_dark_mode());
    ft_switch_on_toggle(dark_sw, on_dark_mode_toggle, NULL);

    /* ─── Floating Mini Status HUD at Bottom-Right ────────────────────── */
    FtWidget status_hud = ft_floating_bar_create(win, FT_ANCHOR_BOTTOM_RIGHT, 240, 32);
    ft_floating_bar_set_margins(status_hud, 18.0, 29.0);
    ft_container_set_padding(status_hud, 10.0, 4.0);
    s_status_text = ft_text_create(status_hud, 0, 0, 220, 22, "Click anywhere on canvas to test!");
    ft_widget_set_style(s_status_text, "font-size: 11px; color: #64748b; text-align: center;");

    update_tab_styles();

    ft_flexbox_update_layout(root);
    ft_flexbox_update_layout(s_manual_top_bar);
    ft_flexbox_update_layout(s_bottom_bar);
    ft_flexbox_update_layout(status_hud);
    ft_widget_show(win);

    printf("[Floating NavBar Example] Entering event loop...\n");
    ft_main_loop();

    ft_quit();
    return 0;
}
