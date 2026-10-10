#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft.h"

static FtWidget s_status_label = NULL;
static int s_suppress_delete_prompt = 0;

static void update_status(const char* text) {
    if (s_status_label) {
        ft_text_set_text(s_status_label, text);
    }
    printf("[STATUS] %s\n", text);
}

/* 1. Information Notice */
static void on_btn_info(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    update_status("Opening Information Dialog...");
    int res = ft_show_info(
        "Application Updated",
        "Floria Toolkit 1.0 is ready",
        "All components, styles, and vector engines have been updated to the latest revision."
    );
    char buf[128];
    snprintf(buf, sizeof(buf), "Information dialog closed (Result: %d)", res);
    update_status(buf);
}

/* 2. Warning Notice */
static void on_btn_warning(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    update_status("Opening Warning Dialog...");
    int res = ft_show_warning(
        "Low Disk Space",
        "Disk space running critically low",
        "You have less than 500 MB remaining on your primary root partition. Consider clearing cache files."
    );
    char buf[128];
    snprintf(buf, sizeof(buf), "Warning dialog closed (Result: %d)", res);
    update_status(buf);
}

/* 3. Error Alert with Technical Details */
static void on_btn_error(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    update_status("Opening Error Dialog with Diagnostic Details...");
    int res = ft_show_error(
        "Database Connection Failed",
        "Unable to establish TLS connection",
        "The database server at db.internal.net refused connection on port 5432.",
        "Diagnostic trace:\n"
        "  connection_state: TLS_HANDSHAKE_TIMEOUT\n"
        "  peer_certificate: UNKNOWN_ISSUER (cert depth 1)\n"
        "  stack_trace:\n"
        "    at ft_net_tls_connect (net/tls.c:142)\n"
        "    at db_pool_acquire (db/pool.c:88)\n"
        "    at main_worker_loop (app/worker.c:304)"
    );
    char buf[128];
    snprintf(buf, sizeof(buf), "Error dialog closed (Result: %d)", res);
    update_status(buf);
}

/* 4. Confirmation (Yes / No) */
static void on_btn_confirm(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    update_status("Opening Confirmation Dialog...");
    int confirmed = ft_confirm(
        "Reset Workspace",
        "Reset all window layouts and toolbars to default?",
        "Any customized docking tabs and floating panels will be reset."
    );
    if (confirmed) {
        update_status("User selected: YES (Confirmed)");
    } else {
        update_status("User selected: NO (Cancelled)");
    }
}

/* 5. Confirmation with "Don't Show Again" Suppression */
static void on_btn_verification(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    if (s_suppress_delete_prompt) {
        update_status("Delete action executed directly (Dialog bypassed by saved 'Don't show again' preference)!");
        return;
    }

    update_status("Opening Confirmation with Suppression Checkbox...");
    int verified = 0;
    int confirmed = ft_confirm_with_verification(
        "Delete Temporary Files",
        "Are you sure you want to delete build artifacts?",
        "This will purge the target/ build cache to reclaim disk space.",
        "Don't show this confirmation again",
        &verified
    );

    char buf[256];
    if (confirmed) {
        if (verified) {
            s_suppress_delete_prompt = 1;
            snprintf(buf, sizeof(buf), "User confirmed: YES (Suppression CHECKED - future prompts will be bypassed)");
        } else {
            snprintf(buf, sizeof(buf), "User confirmed: YES (Suppression unchecked)");
        }
    } else {
        snprintf(buf, sizeof(buf), "User cancelled: NO (Suppression was: %s)", verified ? "checked" : "unchecked");
    }
    update_status(buf);
}

/* Reset Suppression Preference */
static void on_btn_reset_suppression(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    s_suppress_delete_prompt = 0;
    update_status("Suppression reset: Delete confirmation dialog will show again!");
}

/* 6. Custom Multi-Action Dialog (Save / Discard / Cancel) */
static void on_btn_custom_buttons(FtWidget widget, void* user_data) {
    (void)widget; (void)user_data;
    update_status("Opening Custom 3-Button Dialog...");

    FtMessageDialog dlg = ft_message_dialog_create(
        "Unsaved Changes",
        "Save changes to 'project_vector.svg' before closing?",
        "If you close without saving, your recent changes will be permanently discarded.",
        FT_MSG_DIALOG_QUESTION,
        FT_MSG_BUTTONS_CUSTOM
    );

    /* Discard is destructive (red accent) */
    ft_message_dialog_add_button(dlg, "Discard", FT_MODAL_NO, 0, 1);
    /* Cancel is standard secondary */
    ft_message_dialog_add_button(dlg, "Cancel", FT_MODAL_CANCEL, 0, 0);
    /* Save is primary default action (blue accent, Enter key) */
    ft_message_dialog_add_button(dlg, "Save", FT_MODAL_YES, 1, 0);

    int res = ft_message_dialog_show_modal(dlg);
    ft_message_dialog_destroy(dlg);

    if (res == FT_MODAL_YES) {
        update_status("Custom Dialog Result: SAVE (Document saved)");
    } else if (res == FT_MODAL_NO) {
        update_status("Custom Dialog Result: DISCARD (Changes discarded)");
    } else {
        update_status("Custom Dialog Result: CANCEL (Action aborted)");
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("[Message Dialog Demo] Initializing Floria Toolkit...\n");
    ft_init();

    FtWidget win = ft_window_create(640, 520, "Floria Message Dialog & Suppression Showcase");
    ft_window_set_position(win, 100, 100);

    FtWidget root = ft_vbox_create(win, 0, 0, 640, 520);
    ft_container_set_padding(root, 24.0, 24.0);
    ft_flexbox_set_gap(root, 16.0);
    ft_window_set_layout(win, root);

    /* Title & Description */
    FtWidget title_label = ft_text_create(root, 0, 0, 592, 28, "Message Dialog & Alert Demonstration");
    ft_widget_set_font(title_label, "sans-serif,14,bold");
    ft_widget_set_bounds(title_label, 0, 0, 592, 28);

    FtWidget desc_label = ft_text_create(root, 0, 0, 592, 40,
        "Modern modal alerts, confirmation prompts, technical error reporting with expandable details,\n"
        "and 'Don't show this message again' suppression preference handling.");
    ft_widget_set_style(desc_label, "color: #64748b;");
    ft_widget_set_bounds(desc_label, 0, 0, 592, 40);

    /* Row 1: Info, Warning, Error */
    FtWidget row1 = ft_hbox_create(root, 0, 0, 592, 36);
    ft_flexbox_set_gap(row1, 12.0);
    ft_widget_set_bounds(row1, 0, 0, 592, 36);

    FtWidget btn_info = ft_button_create(row1, 0, 0, 185, 36, "Information Dialog");
    ft_button_on_click(btn_info, on_btn_info, NULL);

    FtWidget btn_warn = ft_button_create(row1, 0, 0, 185, 36, "Warning Dialog");
    ft_button_on_click(btn_warn, on_btn_warning, NULL);

    FtWidget btn_err = ft_button_create(row1, 0, 0, 195, 36, "Error with Details");
    ft_button_on_click(btn_err, on_btn_error, NULL);

    /* Row 2: Confirm, Custom */
    FtWidget row2 = ft_hbox_create(root, 0, 0, 592, 36);
    ft_flexbox_set_gap(row2, 12.0);
    ft_widget_set_bounds(row2, 0, 0, 592, 36);

    FtWidget btn_confirm = ft_button_create(row2, 0, 0, 285, 36, "Standard Confirmation (Yes/No)");
    ft_button_on_click(btn_confirm, on_btn_confirm, NULL);

    FtWidget btn_custom = ft_button_create(row2, 0, 0, 295, 36, "Custom 3-Button (Save/Discard/Cancel)");
    ft_button_on_click(btn_custom, on_btn_custom_buttons, NULL);

    /* Row 3: Suppression Checkbox */
    FtWidget row3 = ft_hbox_create(root, 0, 0, 592, 36);
    ft_flexbox_set_gap(row3, 12.0);
    ft_widget_set_bounds(row3, 0, 0, 592, 36);

    FtWidget btn_suppress = ft_button_create(row3, 0, 0, 360, 36, "Confirm with 'Don't Ask Again' Checkbox");
    ft_widget_set_style(btn_suppress, "background-color: #2563eb; color: #ffffff; font-weight: bold; border-color: #1d4ed8;");
    ft_button_on_click(btn_suppress, on_btn_verification, NULL);

    FtWidget btn_reset = ft_button_create(row3, 0, 0, 220, 36, "Reset Saved Preference");
    ft_button_on_click(btn_reset, on_btn_reset_suppression, NULL);

    /* Spacer to push status box to bottom */
    ft_spacer_create(root);

    /* Status Bar Area */
    FtWidget status_box = ft_vbox_create(root, 0, 0, 592, 64);
    ft_container_set_padding(status_box, 12.0, 12.0);
    ft_widget_set_bounds(status_box, 0, 0, 592, 64);
    ft_widget_set_style(status_box, "background-color: rgba(100, 116, 139, 0.08); border-radius: 8px; border: 1px solid rgba(100, 116, 139, 0.2);");

    FtWidget status_header = ft_text_create(status_box, 0, 0, 568, 18, "Action Log / Status:");
    ft_widget_set_style(status_header, "font-size: 11px; font-weight: bold; color: #64748b;");

    s_status_label = ft_text_create(status_box, 0, 0, 568, 22, "Ready. Click any button above to test dialog behaviors.");
    ft_widget_set_style(s_status_label, "font-size: 13px; color: #0284c7;");

    ft_flexbox_update_layout(root);

    ft_widget_show(win);
    printf("[Message Dialog Demo] Window opened, running event loop...\n");
    ft_main_loop();

    ft_quit();
    return 0;
}
