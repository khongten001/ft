#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft.h"

/* Application State */
static FtCamera2D s_camera = NULL;
static FtWidget s_drawing_area = NULL;
static FtWidget s_ruler_h = NULL;
static FtWidget s_ruler_v = NULL;
static FtWidget s_status_coords = NULL;
static FtWidget s_status_zoom = NULL;

static FtWidget s_scrub_x = NULL;
static FtWidget s_scrub_y = NULL;
static FtWidget s_scrub_w = NULL;
static FtWidget s_scrub_h = NULL;
static FtWidget s_scrub_zoom = NULL;

/* Document Artboard dimensions (in World Coordinates) */
static double s_doc_x = 100.0;
static double s_doc_y = 60.0;
static double s_doc_w = 640.0;
static double s_doc_h = 420.0;

/* Editable shape inside document */
static double s_shape_x = 220.0;
static double s_shape_y = 140.0;
static double s_shape_w = 200.0;
static double s_shape_h = 130.0;

/* Mouse navigation state */
static int s_is_panning = 0;
static int s_pan_start_x = 0;
static int s_pan_start_y = 0;
static double s_pan_orig_x = 0.0;
static double s_pan_orig_y = 0.0;
static int s_space_held = 0;

static void sync_rulers_and_status(void) {
    if (!s_camera) return;

    double pan_x = 0.0, pan_y = 0.0;
    ft_camera2d_get_pan(s_camera, &pan_x, &pan_y);
    double zoom = ft_camera2d_get_zoom(s_camera);

    /* Update rulers:
       The document origin (s_doc_x, s_doc_y) maps to screen via WorldToScreen */
    double doc_screen_x = 0.0, doc_screen_y = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_doc_x, s_doc_y, &doc_screen_x, &doc_screen_y);

    if (s_ruler_h) {
        ft_ruler_set_origin_and_scale(s_ruler_h, doc_screen_x, zoom);
    }
    if (s_ruler_v) {
        ft_ruler_set_origin_and_scale(s_ruler_v, doc_screen_y, zoom);
    }

    /* Update Zoom status and scrubber */
    if (s_status_zoom) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Zoom: %d%%", (int)round(zoom * 100.0));
        ft_text_set_text(s_status_zoom, buf);
    }
    if (s_scrub_zoom) {
        ft_numberscrubber_set_value(s_scrub_zoom, zoom * 100.0);
    }
}

/* ─── Drawing Callback ─────────────────────────────────────────────────── */

static void on_paint_canvas(FtWidget widget, FtCanvas canvas, void* user_data) {
    (void)user_data;
    if (!s_camera || !canvas) return;

    int32_t view_w = ft_widget_get_width(widget);
    int32_t view_h = ft_widget_get_height(widget);
    int32_t is_dark = ft_theme_get_dark_mode();

    /* 1. Paint Infinite Pasteboard Background */
    if (is_dark) {
        ft_canvas_draw_rect(canvas, 0, 0, view_w, view_h, 0.11, 0.13, 0.17, 1.0); // slate-900
    } else {
        ft_canvas_draw_rect(canvas, 0, 0, view_w, view_h, 0.90, 0.92, 0.94, 1.0); // slate-200
    }

    double zoom = ft_camera2d_get_zoom(s_camera);

    /* 2. Paint Adaptive Grid in World Coordinates */
    double top_left_wld_x = 0.0, top_left_wld_y = 0.0;
    double bot_right_wld_x = 0.0, bot_right_wld_y = 0.0;
    ft_camera2d_screen_to_world(s_camera, 0.0, 0.0, &top_left_wld_x, &top_left_wld_y);
    ft_camera2d_screen_to_world(s_camera, view_w, view_h, &bot_right_wld_x, &bot_right_wld_y);

    double grid_step = 50.0;
    if (zoom < 0.4) grid_step = 200.0;
    else if (zoom < 0.8) grid_step = 100.0;
    else if (zoom > 2.5) grid_step = 10.0;

    double grid_r = is_dark ? 0.20 : 0.80;
    double grid_g = is_dark ? 0.24 : 0.83;
    double grid_b = is_dark ? 0.30 : 0.86;
    double grid_alpha = 0.4;

    double start_gx = floor(top_left_wld_x / grid_step) * grid_step;
    double end_gx = ceil(bot_right_wld_x / grid_step) * grid_step;
    for (double gx = start_gx; gx <= end_gx; gx += grid_step) {
        double sx = 0.0, sy1 = 0.0, sy2 = 0.0;
        ft_camera2d_world_to_screen(s_camera, gx, top_left_wld_y, &sx, &sy1);
        ft_camera2d_world_to_screen(s_camera, gx, bot_right_wld_y, &sx, &sy2);
        ft_canvas_draw_line(canvas, sx, 0, sx, view_h, 1.0, grid_r, grid_g, grid_b, grid_alpha);
    }

    double start_gy = floor(top_left_wld_y / grid_step) * grid_step;
    double end_gy = ceil(bot_right_wld_y / grid_step) * grid_step;
    for (double gy = start_gy; gy <= end_gy; gy += grid_step) {
        double sy = 0.0, sx1 = 0.0, sx2 = 0.0;
        ft_camera2d_world_to_screen(s_camera, top_left_wld_x, gy, &sx1, &sy);
        ft_camera2d_world_to_screen(s_camera, bot_right_wld_x, gy, &sx2, &sy);
        ft_canvas_draw_line(canvas, 0, sy, view_w, sy, 1.0, grid_r, grid_g, grid_b, grid_alpha);
    }

    /* 3. Paint Document Sheet (White Artboard Page with Drop Shadow) */
    double doc_sx = 0.0, doc_sy = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_doc_x, s_doc_y, &doc_sx, &doc_sy);
    double doc_sw = s_doc_w * zoom;
    double doc_sh = s_doc_h * zoom;

    /* Drop shadow under document */
    ft_canvas_draw_rounded_rect(canvas, doc_sx + 4.0, doc_sy + 4.0, doc_sw, doc_sh, 4.0,
                                0.0, 0.0, 0.0, is_dark ? 0.35 : 0.12);
    ft_canvas_draw_rounded_rect(canvas, doc_sx + 2.0, doc_sy + 2.0, doc_sw, doc_sh, 2.0,
                                0.0, 0.0, 0.0, is_dark ? 0.20 : 0.08);

    /* Artboard paper page */
    if (is_dark) {
        ft_canvas_draw_rect(canvas, doc_sx, doc_sy, doc_sw, doc_sh, 0.18, 0.21, 0.27, 1.0); // slate-800 paper
    } else {
        ft_canvas_draw_rect(canvas, doc_sx, doc_sy, doc_sw, doc_sh, 1.0, 1.0, 1.0, 1.0); // crisp white paper
    }
    /* Document border line */
    ft_canvas_draw_rounded_rect_outline(canvas, doc_sx, doc_sy, doc_sw, doc_sh, 0.0, 1.0,
                                        is_dark ? 0.35 : 0.75,
                                        is_dark ? 0.40 : 0.78,
                                        is_dark ? 0.48 : 0.82, 1.0);

    /* 4. Paint Artboard Content: Header, Vector Shapes, Interactive Shape */
    double header_sx = 0.0, header_sy = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_doc_x + 30.0, s_doc_y + 35.0, &header_sx, &header_sy);
    ft_canvas_draw_text(canvas, header_sx, header_sy, "Floria Vector Document (Artboard 1)", NULL,
                        is_dark ? 0.90 : 0.15, is_dark ? 0.92 : 0.20, is_dark ? 0.96 : 0.25);

    /* Static decorative circle */
    double circ_cx = 0.0, circ_cy = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_doc_x + 100.0, s_doc_y + 180.0, &circ_cx, &circ_cy);
    ft_canvas_draw_circle(canvas, circ_cx, circ_cy, (45.0 + 3.0) * zoom, 1.0, 1.0, 1.0, 0.4);
    ft_canvas_draw_circle(canvas, circ_cx, circ_cy, 45.0 * zoom, 0.93, 0.27, 0.36, 0.85); // rose-500

    /* Interactive Shape (Controlled by scrubbers!) */
    double shp_sx = 0.0, shp_sy = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_shape_x, s_shape_y, &shp_sx, &shp_sy);
    double shp_sw = s_shape_w * zoom;
    double shp_sh = s_shape_h * zoom;

    /* Fill interactive shape with modern vibrant gradient / accent */
    ft_canvas_draw_rounded_rect(canvas, shp_sx, shp_sy, shp_sw, shp_sh, 8.0 * zoom,
                                0.14, 0.45, 0.91, 0.90); // blue-600
    /* Outline & handles */
    ft_canvas_draw_rounded_rect_outline(canvas, shp_sx, shp_sy, shp_sw, shp_sh, 8.0 * zoom, 2.0,
                                        0.38, 0.72, 0.98, 1.0); // sky-400

    /* Shape label */
    double lbl_sx = 0.0, lbl_sy = 0.0;
    ft_camera2d_world_to_screen(s_camera, s_shape_x + 16.0, s_shape_y + 24.0, &lbl_sx, &lbl_sy);
    ft_canvas_draw_text(canvas, lbl_sx, lbl_sy, "Interactive Vector Card", NULL, 1.0, 1.0, 1.0);

    /* 4 Corner selection gizmo handles */
    double h_rad = 4.0;
    ft_canvas_draw_rect(canvas, shp_sx - h_rad, shp_sy - h_rad, h_rad * 2, h_rad * 2, 1.0, 1.0, 1.0, 1.0);
    ft_canvas_draw_rect(canvas, shp_sx + shp_sw - h_rad, shp_sy - h_rad, h_rad * 2, h_rad * 2, 1.0, 1.0, 1.0, 1.0);
    ft_canvas_draw_rect(canvas, shp_sx - h_rad, shp_sy + shp_sh - h_rad, h_rad * 2, h_rad * 2, 1.0, 1.0, 1.0, 1.0);
    ft_canvas_draw_rect(canvas, shp_sx + shp_sw - h_rad, shp_sy + shp_sh - h_rad, h_rad * 2, h_rad * 2, 1.0, 1.0, 1.0, 1.0);
}

/* ─── Mouse Navigation Callbacks ───────────────────────────────────────── */

static void on_mouse_down_canvas(FtWidget widget, int32_t x, int32_t y, int32_t button, void* user_data) {
    (void)widget; (void)user_data;
    /* Middle button (2) or Left click with spacebar initiates panning */
    if (button == 2 || (button == 1 && s_space_held)) {
        s_is_panning = 1;
        s_pan_start_x = x;
        s_pan_start_y = y;
        ft_camera2d_get_pan(s_camera, &s_pan_orig_x, &s_pan_orig_y);
        ft_drawingarea_set_cursor(s_drawing_area, FT_CURSOR_MOVE);
    }
}

static void on_mouse_move_canvas(FtWidget widget, int32_t x, int32_t y, void* user_data) {
    (void)user_data;
    if (s_is_panning) {
        int dx = x - s_pan_start_x;
        int dy = y - s_pan_start_y;
        ft_camera2d_set_pan(s_camera, s_pan_orig_x + dx, s_pan_orig_y + dy);
        sync_rulers_and_status();
        ft_widget_invalidate(widget);
    }

    /* Update ruler pointer tracking hairlines */
    if (s_ruler_h) ft_ruler_set_pointer_position(s_ruler_h, (double)x);
    if (s_ruler_v) ft_ruler_set_pointer_position(s_ruler_v, (double)y);

    /* Update World coordinates in status bar */
    if (s_camera && s_status_coords) {
        double wx = 0.0, wy = 0.0;
        ft_camera2d_screen_to_world(s_camera, (double)x, (double)y, &wx, &wy);
        /* Coordinates relative to document origin */
        double rel_doc_x = wx - s_doc_x;
        double rel_doc_y = wy - s_doc_y;
        char buf[128];
        snprintf(buf, sizeof(buf), "Doc: (%.1f, %.1f) px | World: (%.1f, %.1f)", rel_doc_x, rel_doc_y, wx, wy);
        ft_text_set_text(s_status_coords, buf);
    }
}

static void on_resize_canvas(FtWidget widget, int32_t new_width, int32_t new_height, void* user_data) {
    (void)widget; (void)new_width; (void)new_height; (void)user_data;
    sync_rulers_and_status();
}

static void on_mouse_up_canvas(FtWidget widget, int32_t x, int32_t y, int32_t button, void* user_data) {
    (void)x; (void)y; (void)user_data;
    if (s_is_panning && (button == 2 || button == 1)) {
        s_is_panning = 0;
        if (s_space_held)
            ft_drawingarea_set_cursor(s_drawing_area, FT_CURSOR_HAND);
        else
            ft_drawingarea_set_cursor(s_drawing_area, FT_CURSOR_DEFAULT);
    }
}

static void on_mouse_wheel_canvas(FtWidget widget, int32_t x, int32_t y, int32_t delta_x, int32_t delta_y, void* user_data) {
    (void)delta_x; (void)user_data;
    if (!s_camera) return;

    /* Zoom in/out anchored on cursor position! */
    double factor = (delta_y > 0) ? 1.15 : (1.0 / 1.15);
    ft_camera2d_zoom_at(s_camera, (double)x, (double)y, factor);

    sync_rulers_and_status();
    ft_widget_invalidate(widget);
}

static void on_key_down_canvas(FtWidget widget, uint32_t keysym, uint32_t state, void* user_data) {
    (void)state; (void)user_data;
    // Spacebar (XK_space = 32)
    if (keysym == 32) {
        s_space_held = 1;
        ft_drawingarea_set_cursor(s_drawing_area, FT_CURSOR_HAND);
    }
}

static void on_key_up_canvas(FtWidget widget, uint32_t keysym, uint32_t state, void* user_data) {
    (void)state; (void)user_data;
    if (keysym == 32) {
        s_space_held = 0;
        if (!s_is_panning)
            ft_drawingarea_set_cursor(s_drawing_area, FT_CURSOR_DEFAULT);
    }
}

/* ─── Toolbar Action Handlers ──────────────────────────────────────────── */

static void on_btn_zoom_in(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (!s_camera || !s_drawing_area) return;
    int w = ft_widget_get_width(s_drawing_area);
    int h = ft_widget_get_height(s_drawing_area);
    ft_camera2d_zoom_at(s_camera, w * 0.5, h * 0.5, 1.25);
    sync_rulers_and_status();
    ft_widget_invalidate(s_drawing_area);
}

static void on_btn_zoom_out(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (!s_camera || !s_drawing_area) return;
    int w = ft_widget_get_width(s_drawing_area);
    int h = ft_widget_get_height(s_drawing_area);
    ft_camera2d_zoom_at(s_camera, w * 0.5, h * 0.5, 0.80);
    sync_rulers_and_status();
    ft_widget_invalidate(s_drawing_area);
}

static void on_btn_zoom_reset(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (!s_camera || !s_drawing_area) return;
    int w = ft_widget_get_width(s_drawing_area);
    int h = ft_widget_get_height(s_drawing_area);
    /* Center document at 100% zoom */
    double center_doc_x = s_doc_x + s_doc_w * 0.5;
    double center_doc_y = s_doc_y + s_doc_h * 0.5;
    ft_camera2d_set_zoom(s_camera, 1.0);
    ft_camera2d_set_pan(s_camera, (w * 0.5) - center_doc_x, (h * 0.5) - center_doc_y);
    sync_rulers_and_status();
    ft_widget_invalidate(s_drawing_area);
}

static void on_btn_fit_doc(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (!s_camera || !s_drawing_area) return;
    int w = ft_widget_get_width(s_drawing_area);
    int h = ft_widget_get_height(s_drawing_area);
    ft_camera2d_fit_rect(s_camera, s_doc_x, s_doc_y, s_doc_w, s_doc_h, (double)w, (double)h, 40.0);
    sync_rulers_and_status();
    ft_widget_invalidate(s_drawing_area);
}

/* Scrubber change handlers */
static void on_scrub_x(FtWidget widget, double val, void* user_data) {
    (void)widget; (void)user_data;
    s_shape_x = val;
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
}

static void on_scrub_y(FtWidget widget, double val, void* user_data) {
    (void)widget; (void)user_data;
    s_shape_y = val;
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
}

static void on_scrub_w(FtWidget widget, double val, void* user_data) {
    (void)widget; (void)user_data;
    s_shape_w = val;
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
}

static void on_scrub_h(FtWidget widget, double val, void* user_data) {
    (void)widget; (void)user_data;
    s_shape_h = val;
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
}

static void on_scrub_zoom(FtWidget widget, double val, void* user_data) {
    (void)widget; (void)user_data;
    if (!s_camera || !s_drawing_area) return;
    int w = ft_widget_get_width(s_drawing_area);
    int h = ft_widget_get_height(s_drawing_area);
    ft_camera2d_set_zoom_at(s_camera, w * 0.5, h * 0.5, val / 100.0);
    sync_rulers_and_status();
    ft_widget_invalidate(s_drawing_area);
}

static void on_unit_px(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (s_ruler_h) ft_ruler_set_unit(s_ruler_h, FT_RULER_UNIT_PIXELS);
    if (s_ruler_v) ft_ruler_set_unit(s_ruler_v, FT_RULER_UNIT_PIXELS);
}

static void on_unit_in(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (s_ruler_h) ft_ruler_set_unit(s_ruler_h, FT_RULER_UNIT_INCHES);
    if (s_ruler_v) ft_ruler_set_unit(s_ruler_v, FT_RULER_UNIT_INCHES);
}

static void on_unit_mm(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (s_ruler_h) ft_ruler_set_unit(s_ruler_h, FT_RULER_UNIT_MILLIMETERS);
    if (s_ruler_v) ft_ruler_set_unit(s_ruler_v, FT_RULER_UNIT_MILLIMETERS);
}

static void on_dark_mode_toggle(FtWidget widget, int32_t active, void* user_data) {
    (void)widget; (void)user_data;
    ft_theme_set_dark_mode(active);
    if (s_drawing_area) ft_widget_invalidate(s_drawing_area);
    if (s_ruler_h) ft_widget_invalidate(s_ruler_h);
    if (s_ruler_v) ft_widget_invalidate(s_ruler_v);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("[Creative Canvas] Initializing Floria Toolkit...\n");
    ft_init();

    s_camera = ft_camera2d_create(0.05, 32.0);

    FtWidget win = ft_window_create(960, 680, "Floria Creative Viewport & Ruler Studio");
    ft_window_set_position(win, 60, 60);

    /* Root vertical layout */
    FtWidget root = ft_vbox_create(win, 0, 0, 960, 680);
    ft_window_set_layout(win, root);

    /* ─── Top Main Toolbar (Navigation & Global Actions) ─────────────── */
    FtWidget top_bar = ft_hbox_create(root, 0, 0, 960, 42);
    ft_widget_set_bounds(top_bar, 0, 0, 960, 42);
    ft_container_set_padding(top_bar, 12.0, 4.0);
    ft_flexbox_set_gap(top_bar, 8.0);
    ft_widget_set_style(top_bar, "background-color: rgba(100, 116, 139, 0.08); border-bottom: 1px solid rgba(100, 116, 139, 0.2);");

    FtWidget title_lbl = ft_text_create(top_bar, 0, 0, 170, 28, "Creative Viewport");
    ft_widget_set_font(title_lbl, "sans-serif,12,bold");

    /* Zoom Buttons */
    FtWidget btn_zin = ft_button_create(top_bar, 0, 0, 34, 28, "+");
    ft_button_on_click(btn_zin, on_btn_zoom_in, NULL);

    FtWidget btn_zout = ft_button_create(top_bar, 0, 0, 34, 28, "-");
    ft_button_on_click(btn_zout, on_btn_zoom_out, NULL);

    FtWidget btn_z100 = ft_button_create(top_bar, 0, 0, 52, 28, "100%");
    ft_button_on_click(btn_z100, on_btn_zoom_reset, NULL);

    FtWidget btn_fit = ft_button_create(top_bar, 0, 0, 72, 28, "Fit Page");
    ft_button_on_click(btn_fit, on_btn_fit_doc, NULL);

    /* Unit buttons */
    FtWidget btn_px = ft_button_create(top_bar, 0, 0, 36, 28, "px");
    ft_button_on_click(btn_px, on_unit_px, NULL);

    FtWidget btn_in = ft_button_create(top_bar, 0, 0, 36, 28, "in");
    ft_button_on_click(btn_in, on_unit_in, NULL);

    FtWidget btn_mm = ft_button_create(top_bar, 0, 0, 40, 28, "mm");
    ft_button_on_click(btn_mm, on_unit_mm, NULL);

    ft_spacer_create(top_bar);

    FtWidget dark_lbl = ft_text_create(top_bar, 0, 0, 85, 28, "Dark Mode");
    (void)dark_lbl;
    FtWidget dark_sw = ft_switch_create(top_bar, 0, 0, 48, 24, "");
    ft_switch_set_checked(dark_sw, ft_theme_get_dark_mode());
    ft_switch_on_toggle(dark_sw, on_dark_mode_toggle, NULL);

    /* ─── Secondary Property Bar (Photoshop-style Scrubbers) ──────────── */
    FtWidget prop_bar = ft_hbox_create(root, 0, 0, 960, 38);
    ft_widget_set_bounds(prop_bar, 0, 0, 960, 38);
    ft_container_set_padding(prop_bar, 12.0, 4.0);
    ft_flexbox_set_gap(prop_bar, 10.0);
    ft_widget_set_style(prop_bar, "background-color: rgba(100, 116, 139, 0.04); border-bottom: 1px solid rgba(100, 116, 139, 0.15);");

    FtWidget card_lbl = ft_text_create(prop_bar, 0, 0, 100, 26, "Vector Card:");
    ft_widget_set_style(card_lbl, "font-size: 11px; font-weight: bold; color: #64748b;");

    s_scrub_x = ft_numberscrubber_create(prop_bar, 0, 0, 95, 26, "X");
    ft_numberscrubber_set_value(s_scrub_x, s_shape_x);
    ft_numberscrubber_set_suffix(s_scrub_x, "px");
    ft_numberscrubber_on_change(s_scrub_x, on_scrub_x, NULL);

    s_scrub_y = ft_numberscrubber_create(prop_bar, 0, 0, 95, 26, "Y");
    ft_numberscrubber_set_value(s_scrub_y, s_shape_y);
    ft_numberscrubber_set_suffix(s_scrub_y, "px");
    ft_numberscrubber_on_change(s_scrub_y, on_scrub_y, NULL);

    s_scrub_w = ft_numberscrubber_create(prop_bar, 0, 0, 95, 26, "W");
    ft_numberscrubber_set_value(s_scrub_w, s_shape_w);
    ft_numberscrubber_set_suffix(s_scrub_w, "px");
    ft_numberscrubber_on_change(s_scrub_w, on_scrub_w, NULL);

    s_scrub_h = ft_numberscrubber_create(prop_bar, 0, 0, 95, 26, "H");
    ft_numberscrubber_set_value(s_scrub_h, s_shape_h);
    ft_numberscrubber_set_suffix(s_scrub_h, "px");
    ft_numberscrubber_on_change(s_scrub_h, on_scrub_h, NULL);

    ft_spacer_create(prop_bar);

    FtWidget zoom_lbl = ft_text_create(prop_bar, 0, 0, 50, 26, "Zoom:");
    ft_widget_set_style(zoom_lbl, "font-size: 11px; color: #64748b;");

    s_scrub_zoom = ft_numberscrubber_create(prop_bar, 0, 0, 85, 26, "");
    ft_numberscrubber_set_range(s_scrub_zoom, 5.0, 3200.0);
    ft_numberscrubber_set_step(s_scrub_zoom, 5.0);
    ft_numberscrubber_set_suffix(s_scrub_zoom, "%");
    ft_numberscrubber_set_value(s_scrub_zoom, 100.0);
    ft_numberscrubber_on_change(s_scrub_zoom, on_scrub_zoom, NULL);

    /* ─── Central Workspace: Rulers + Drawing Area ───────────────────── */
    /* Workspace container (VBox for top ruler + row container) */
    FtWidget work_vbox = ft_vbox_create(root, 0, 0, 960, 560);
    ft_widget_set_flex_grow(work_vbox, 1.0);

    /* Row 1 of workspace: Corner square (20x20) + Top Horizontal Ruler */
    FtWidget top_ruler_row = ft_hbox_create(work_vbox, 0, 0, 960, 20);
    ft_widget_set_bounds(top_ruler_row, 0, 0, 960, 20);

    /* Corner block (20x20) where horizontal & vertical rulers meet */
    FtWidget corner_box = ft_container_create(top_ruler_row, 0, 0, 20, 20);
    ft_widget_set_bounds(corner_box, 0, 0, 20, 20);
    ft_widget_set_style(corner_box, "background-color: rgba(100, 116, 139, 0.2); border-right: 1px solid rgba(100, 116, 139, 0.3); border-bottom: 1px solid rgba(100, 116, 139, 0.3);");

    s_ruler_h = ft_ruler_create(top_ruler_row, FT_RULER_HORIZONTAL, 0, 0, 940, 20);
    ft_widget_set_flex_grow(s_ruler_h, 1.0);

    /* Row 2 of workspace: Left Vertical Ruler + Central Drawing Area */
    FtWidget main_canvas_row = ft_hbox_create(work_vbox, 0, 0, 960, 540);
    ft_widget_set_flex_grow(main_canvas_row, 1.0);

    s_ruler_v = ft_ruler_create(main_canvas_row, FT_RULER_VERTICAL, 0, 0, 20, 540);
    ft_widget_set_bounds(s_ruler_v, 0, 0, 20, 540);

    /* Blank drawing area for our application canvas! */
    s_drawing_area = ft_drawingarea_create(main_canvas_row, 0, 0, 940, 540);
    ft_widget_set_flex_grow(s_drawing_area, 1.0);
    ft_drawingarea_on_paint(s_drawing_area, on_paint_canvas, NULL);
    ft_drawingarea_on_mouse_down(s_drawing_area, on_mouse_down_canvas, NULL);
    ft_drawingarea_on_mouse_move(s_drawing_area, on_mouse_move_canvas, NULL);
    ft_drawingarea_on_mouse_up(s_drawing_area, on_mouse_up_canvas, NULL);
    ft_drawingarea_on_mouse_wheel(s_drawing_area, on_mouse_wheel_canvas, NULL);
    ft_drawingarea_on_key_down(s_drawing_area, on_key_down_canvas, NULL);
    ft_drawingarea_on_key_up(s_drawing_area, on_key_up_canvas, NULL);
    ft_drawingarea_on_resize(s_drawing_area, on_resize_canvas, NULL);

    /* ─── Bottom Status Bar ─────────────────────────────────────────── */
    FtWidget status_bar = ft_hbox_create(root, 0, 0, 960, 26);
    ft_widget_set_bounds(status_bar, 0, 0, 960, 26);
    ft_container_set_padding(status_bar, 12.0, 3.0);
    ft_widget_set_style(status_bar, "background-color: rgba(100, 116, 139, 0.08); border-top: 1px solid rgba(100, 116, 139, 0.2);");

    s_status_coords = ft_text_create(status_bar, 0, 0, 360, 20, "Doc: (0.0, 0.0) px | World: (0.0, 0.0)");
    ft_widget_set_style(s_status_coords, "font-size: 11px; color: #64748b;");

    ft_spacer_create(status_bar);

    s_status_zoom = ft_text_create(status_bar, 0, 0, 120, 20, "Zoom: 100%");
    ft_widget_set_style(s_status_zoom, "font-size: 11px; color: #0284c7; text-align: right;");

    ft_flexbox_update_layout(root);
    /* Initial center fit view with computed layout geometry */
    on_btn_fit_doc(NULL, NULL);

    ft_widget_show(win);

    printf("[Creative Canvas] Running event loop...\n");
    ft_main_loop();

    ft_camera2d_destroy(s_camera);
    ft_quit();
    return 0;
}
