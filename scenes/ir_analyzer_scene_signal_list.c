#include "ir_analyzer_scene.h"

static void ir_analyzer_submenu_cb(void* ctx, uint32_t index) {
    IrAnalyzerApp* app = ctx;
    app->selected_signal = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void ir_analyzer_scene_signal_list_on_enter(void* ctx) {
    IrAnalyzerApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Signaux captures");

    if(app->signal_count == 0) {
        submenu_add_item(app->submenu, "(aucun signal)", 0, NULL, NULL);
    } else {
        for(uint32_t i = 0; i < app->signal_count; i++) {
            IrAnalyzerSignal* sig = &app->signals[i];
            char label[40];
            if(!sig->is_raw) {
                snprintf(label, sizeof(label), "#%u %s 0x%04X",
                    (unsigned int)(i + 1), sig->protocol, (unsigned int)sig->command);
            } else {
                snprintf(label, sizeof(label), "#%u RAW (%u t)",
                    (unsigned int)(i + 1), (unsigned int)sig->raw_count);
            }
            submenu_add_item(app->submenu, label, i, ir_analyzer_submenu_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, IrAnalyzerViewSignalList);
}

bool ir_analyzer_scene_signal_list_on_event(void* ctx, SceneManagerEvent event) {
    IrAnalyzerApp* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        app->selected_signal = event.event;
        scene_manager_next_scene(app->scene_manager, IrAnalyzerSceneSignalDetail);
        consumed = true;
    }
    return consumed;
}

void ir_analyzer_scene_signal_list_on_exit(void* ctx) {
    IrAnalyzerApp* app = ctx;
    submenu_reset(app->submenu);
    UNUSED(app);
}
