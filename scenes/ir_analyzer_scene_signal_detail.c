#include "ir_analyzer_scene.h"

void ir_analyzer_scene_signal_detail_on_enter(void* ctx) {
    IrAnalyzerApp* app = ctx;
    IrAnalyzerSignal* sig = &app->signals[app->selected_signal];
    char buf[48];

    widget_reset(app->widget_detail);

    snprintf(buf, sizeof(buf), "Signal #%u", (unsigned int)(app->selected_signal + 1));
    widget_add_string_element(app->widget_detail, 64, 0, AlignCenter, AlignTop, FontPrimary, buf);

    widget_add_string_element(app->widget_detail, 0, 14, AlignLeft, AlignTop,
                              FontSecondary, "Protocole:");
    widget_add_string_element(app->widget_detail, 0, 23, AlignLeft, AlignTop,
                              FontPrimary, sig->protocol);

    if(!sig->is_raw) {
        snprintf(buf, sizeof(buf), "Addr : 0x%04X", (unsigned int)sig->address);
        widget_add_string_element(app->widget_detail, 0, 36, AlignLeft, AlignTop, FontSecondary, buf);

        snprintf(buf, sizeof(buf), "Cmd  : 0x%04X", (unsigned int)sig->command);
        widget_add_string_element(app->widget_detail, 0, 46, AlignLeft, AlignTop, FontSecondary, buf);

        snprintf(buf, sizeof(buf), "Repeat: %s", sig->repeat ? "Oui" : "Non");
        widget_add_string_element(app->widget_detail, 0, 56, AlignLeft, AlignTop, FontSecondary, buf);
    } else {
        snprintf(buf, sizeof(buf), "RAW: %u timings", (unsigned int)sig->raw_count);
        widget_add_string_element(app->widget_detail, 0, 36, AlignLeft, AlignTop, FontSecondary, buf);
        if(sig->raw_count > 0) {
            snprintf(buf, sizeof(buf), "1er: %uus", (unsigned int)sig->raw_timings[0]);
            widget_add_string_element(app->widget_detail, 0, 48, AlignLeft, AlignTop, FontSecondary, buf);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, IrAnalyzerViewSignalDetail);
}

bool ir_analyzer_scene_signal_detail_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void ir_analyzer_scene_signal_detail_on_exit(void* ctx) {
    IrAnalyzerApp* app = ctx;
    widget_reset(app->widget_detail);
    UNUSED(app);
}
