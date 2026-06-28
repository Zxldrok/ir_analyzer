#include "ir_analyzer_scene.h"

static void ir_analyzer_main_draw_cb(Canvas* canvas, void* ctx) {
    furi_assert(canvas);
    furi_assert(ctx);
    IrAnalyzerApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "IR Analyzer");
    canvas_draw_line(canvas, 0, 13, 127, 13);
    canvas_set_font(canvas, FontSecondary);

    if(app->signal_received && app->signal_count > 0) {
        IrAnalyzerSignal* sig = &app->signals[app->signal_count - 1];
        char buf[32];

        canvas_draw_str(canvas, 2, 25, "Proto:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 40, 25, sig->protocol);
        canvas_set_font(canvas, FontSecondary);

        if(!sig->is_raw) {
            snprintf(buf, sizeof(buf), "Addr: 0x%04X", (unsigned int)sig->address);
            canvas_draw_str(canvas, 2, 38, buf);
            snprintf(buf, sizeof(buf), "Cmd:  0x%04X", (unsigned int)sig->command);
            canvas_draw_str(canvas, 2, 49, buf);
            canvas_draw_str(canvas, 2, 60, sig->repeat ? "Repeat: Oui" : "Repeat: Non");
        } else {
            snprintf(buf, sizeof(buf), "RAW: %u timings", (unsigned int)sig->raw_count);
            canvas_draw_str(canvas, 2, 38, buf);
        }
    } else {
        canvas_draw_str(canvas, 4, 30, "En attente d'un signal...");
        canvas_draw_str(canvas, 4, 42, "Pointe une telecommande");
        canvas_draw_str(canvas, 4, 54, "vers le port IR");
    }

    canvas_draw_str(canvas, 0, 63, "OK:liste  BACK:quitter");
}

static bool ir_analyzer_main_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    IrAnalyzerApp* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        return true;
    }
    return false;
}

void ir_analyzer_scene_main_on_enter(void* ctx) {
    furi_assert(ctx);
    IrAnalyzerApp* app = ctx;
    view_set_draw_callback(app->view_main, ir_analyzer_main_draw_cb);
    view_set_input_callback(app->view_main, ir_analyzer_main_input_cb);
    view_set_context(app->view_main, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrAnalyzerViewMain);
}

bool ir_analyzer_scene_main_on_event(void* ctx, SceneManagerEvent event) {
    furi_assert(ctx);
    IrAnalyzerApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0) {
            // Nouveau signal : forcer un redraw
            view_dispatcher_switch_to_view(app->view_dispatcher, IrAnalyzerViewMain);
            consumed = true;
        } else if(event.event == 1) {
            scene_manager_next_scene(app->scene_manager, IrAnalyzerSceneSignalList);
            consumed = true;
        }
    }
    return consumed;
}

void ir_analyzer_scene_main_on_exit(void* ctx) {
    UNUSED(ctx);
}
