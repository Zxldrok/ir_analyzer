#include "ir_analyzer.h"
#include "scenes/ir_analyzer_scene.h"

static bool ir_analyzer_custom_event_cb(void* ctx, uint32_t event) {
    IrAnalyzerApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ir_analyzer_back_event_cb(void* ctx) {
    IrAnalyzerApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

void ir_analyzer_signal_callback(void* ctx, InfraredWorkerSignal* received_signal) {
    furi_assert(ctx);
    IrAnalyzerApp* app = ctx;
    if(app->signal_count >= IR_ANALYZER_MAX_SIGNALS) return;

    IrAnalyzerSignal* sig = &app->signals[app->signal_count];
    memset(sig, 0, sizeof(IrAnalyzerSignal));

    if(infrared_worker_signal_is_decoded(received_signal)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received_signal);
        strncpy(
            sig->protocol,
            infrared_get_protocol_name(msg->protocol),
            sizeof(sig->protocol) - 1);
        sig->address = msg->address;
        sig->command = msg->command;
        sig->repeat  = msg->repeat;
        sig->is_raw  = false;
    } else {
        const uint32_t* timings;
        size_t timings_cnt;
        infrared_worker_get_raw_signal(received_signal, &timings, &timings_cnt);
        strncpy(sig->protocol, "RAW", sizeof(sig->protocol) - 1);
        sig->is_raw = true;
        uint32_t n = (timings_cnt < 256) ? (uint32_t)timings_cnt : 256;
        memcpy(sig->raw_timings, timings, n * sizeof(uint32_t));
        sig->raw_count = n;
    }

    app->signal_count++;
    app->signal_received = true;
    notification_message(app->notifications, &sequence_blink_green_10);
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

IrAnalyzerApp* ir_analyzer_app_alloc(void) {
    IrAnalyzerApp* app = malloc(sizeof(IrAnalyzerApp));
    furi_check(app);
    memset(app, 0, sizeof(IrAnalyzerApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager   = scene_manager_alloc(&ir_analyzer_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ir_analyzer_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, ir_analyzer_back_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->view_main = view_alloc();
    view_set_context(app->view_main, app);
    view_dispatcher_add_view(app->view_dispatcher, IrAnalyzerViewMain, app->view_main);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IrAnalyzerViewSignalList, submenu_get_view(app->submenu));

    app->widget_detail = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, IrAnalyzerViewSignalDetail, widget_get_view(app->widget_detail));

    // Worker IR — callback déclaré mais worker PAS démarré ici
    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(
        app->ir_worker, ir_analyzer_signal_callback, app);

    return app;
}

void ir_analyzer_app_free(IrAnalyzerApp* app) {
    furi_assert(app);
    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);

    view_dispatcher_remove_view(app->view_dispatcher, IrAnalyzerViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, IrAnalyzerViewSignalList);
    view_dispatcher_remove_view(app->view_dispatcher, IrAnalyzerViewSignalDetail);

    view_free(app->view_main);
    submenu_free(app->submenu);
    widget_free(app->widget_detail);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t ir_analyzer_app(void* p) {
    UNUSED(p);
    IrAnalyzerApp* app = ir_analyzer_app_alloc();
    scene_manager_next_scene(app->scene_manager, IrAnalyzerSceneMain);
    // Worker démarré APRÈS que la scène est prête
    infrared_worker_rx_start(app->ir_worker);
    view_dispatcher_run(app->view_dispatcher);
    ir_analyzer_app_free(app);
    return 0;
}
