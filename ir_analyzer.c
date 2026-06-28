#include "ir_analyzer.h"

typedef enum {
    EventTypeInput,
    EventTypeIrSignal,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    union {
        InputEvent input;
        uint32_t   signal_index;
    };
} AppEvent;

// ── Callback IR ───────────────────────────────────────────────────────────────
static void ir_signal_callback(void* ctx, InfraredWorkerSignal* received) {
    IrAnalyzerApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->signal_count >= IR_ANALYZER_MAX_SIGNALS) {
        furi_mutex_release(app->mutex);
        return;
    }

    IrAnalyzerSignal* sig = &app->signals[app->signal_count];
    memset(sig, 0, sizeof(IrAnalyzerSignal));

    if(infrared_worker_signal_is_decoded(received)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received);
        strncpy(sig->protocol, infrared_get_protocol_name(msg->protocol), sizeof(sig->protocol) - 1);
        sig->address = msg->address;
        sig->command = msg->command;
        sig->repeat  = msg->repeat;
        sig->is_raw  = false;
    } else {
        const uint32_t* timings;
        size_t count;
        infrared_worker_get_raw_signal(received, &timings, &count);
        strncpy(sig->protocol, "RAW", sizeof(sig->protocol) - 1);
        sig->is_raw    = true;
        sig->raw_count = (uint32_t)count;
    }

    app->signal_count++;
    furi_mutex_release(app->mutex);

    AppEvent event = {.type = EventTypeIrSignal};
    furi_message_queue_put(app->event_queue, &event, 0);
    notification_message(app->notifications, &sequence_blink_green_10);
}

// ── Callback input ────────────────────────────────────────────────────────────
static void input_callback(InputEvent* input_event, void* ctx) {
    IrAnalyzerApp* app = ctx;
    AppEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// ── Rendu ─────────────────────────────────────────────────────────────────────
static void draw_callback(Canvas* canvas, void* ctx) {
    IrAnalyzerApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "IR Analyzer");
    canvas_draw_line(canvas, 0, 13, 127, 13);
    canvas_set_font(canvas, FontSecondary);

    if(app->signal_count == 0) {
        canvas_draw_str(canvas, 4, 30, "En attente d'un signal...");
        canvas_draw_str(canvas, 4, 42, "Pointe une telecommande");
        canvas_draw_str(canvas, 4, 54, "vers le port IR");
    } else {
        IrAnalyzerSignal* sig = &app->signals[app->signal_count - 1];
        char buf[32];

        canvas_draw_str(canvas, 2, 25, "Proto:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 42, 25, sig->protocol);
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
    }

    canvas_set_font(canvas, FontSecondary);
    char count_buf[24];
    snprintf(count_buf, sizeof(count_buf), "Total: %u", (unsigned int)app->signal_count);
    canvas_draw_str(canvas, 0, 62, count_buf);
    furi_mutex_release(app->mutex);
}

// ── Point d'entrée ────────────────────────────────────────────────────────────
int32_t ir_analyzer_app(void* p) {
    UNUSED(p);

    IrAnalyzerApp* app = malloc(sizeof(IrAnalyzerApp));
    furi_check(app);
    memset(app, 0, sizeof(IrAnalyzerApp));

    app->event_queue  = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->mutex        = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui          = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ir_signal_callback, app);
    infrared_worker_rx_start(app->ir_worker);

    app->running = true;
    AppEvent event;

    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input.type == InputTypeShort && event.input.key == InputKeyBack) {
                    app->running = false;
                }
            } else if(event.type == EventTypeIrSignal) {
                view_port_update(app->view_port);
            }
        }
    }

    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
