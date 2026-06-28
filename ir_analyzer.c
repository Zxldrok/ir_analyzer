#include "ir_analyzer.h"

typedef enum {
    EventTypeInput,
    EventTypeIrSignal,
} EventType;

typedef struct {
    EventType  type;
    union {
        InputEvent input;
    };
} AppEvent;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool signals_match(IrAnalyzerSignal* a, IrAnalyzerSignal* b) {
    if(a->is_raw != b->is_raw) return false;
    if(a->is_raw) {
        if(a->raw_count != b->raw_count) return false;
        for(uint32_t i = 0; i < a->raw_count && i < 8; i++) {
            uint32_t diff = a->raw_timings[i] > b->raw_timings[i]
                ? a->raw_timings[i] - b->raw_timings[i]
                : b->raw_timings[i] - a->raw_timings[i];
            if(diff > 200) return false;
        }
        return true;
    }
    return strcmp(a->protocol, b->protocol) == 0
        && a->address == b->address
        && a->command  == b->command;
}

static bool save_signal(IrAnalyzerSignal* sig, uint32_t index) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/infrared");
    storage_common_mkdir(storage, IR_SAVE_PATH);

    char path[128];
    if(sig->is_raw) {
        snprintf(path, sizeof(path), IR_SAVE_PATH "/signal_%lu_RAW.ir", index);
    } else {
        snprintf(path, sizeof(path), IR_SAVE_PATH "/signal_%lu_%s.ir", index, sig->protocol);
    }

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        int len;

        len = snprintf(buf, sizeof(buf), "Filetype: IR signals file\nVersion: 1\n#\n");
        storage_file_write(file, buf, len);

        if(sig->is_raw) {
            len = snprintf(buf, sizeof(buf),
                "name: RAW_%lu\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata:");
            storage_file_write(file, buf, len);
            uint32_t lim = sig->raw_count < IR_ANALYZER_MAX_RAW ? sig->raw_count : IR_ANALYZER_MAX_RAW;
            for(uint32_t i = 0; i < lim; i++) {
                len = snprintf(buf, sizeof(buf), " %lu", sig->raw_timings[i]);
                storage_file_write(file, buf, len);
            }
            storage_file_write(file, "\n", 1);
        } else {
            len = snprintf(buf, sizeof(buf),
                "name: %s_0x%04lX\ntype: parsed\nprotocol: %s\naddress: 0x%04lX\ncommand: 0x%04lX\n",
                sig->protocol, sig->command, sig->protocol, sig->address, sig->command);
            storage_file_write(file, buf, len);
        }
        ok = true;
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ── Callback IR ───────────────────────────────────────────────────────────────
static void ir_signal_callback(void* ctx, InfraredWorkerSignal* received) {
    IrAnalyzerApp* app = ctx;
    if(!app) return;

    IrAnalyzerSignal tmp;
    memset(&tmp, 0, sizeof(tmp));

    if(infrared_worker_signal_is_decoded(received)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received);
        strncpy(tmp.protocol, infrared_get_protocol_name(msg->protocol), sizeof(tmp.protocol) - 1);
        tmp.address = msg->address;
        tmp.command = msg->command;
        tmp.repeat  = msg->repeat;
        tmp.is_raw  = false;
    } else {
        const uint32_t* timings;
        size_t count;
        infrared_worker_get_raw_signal(received, &timings, &count);
        strncpy(tmp.protocol, "RAW", sizeof(tmp.protocol) - 1);
        tmp.is_raw    = true;
        tmp.raw_count = count < IR_ANALYZER_MAX_RAW ? (uint32_t)count : IR_ANALYZER_MAX_RAW;
        memcpy(tmp.raw_timings, timings, tmp.raw_count * sizeof(uint32_t));
    }

    // Déduplication
    for(uint32_t i = 0; i < app->signal_count; i++) {
        if(signals_match(&app->signals[i], &tmp)) {
            app->signals[i].seen_count++;
            AppEvent event = {.type = EventTypeIrSignal};
            furi_message_queue_put(app->event_queue, &event, 0);
            if(app->notifications)
                notification_message(app->notifications, &sequence_blink_yellow_10);
            return;
        }
    }

    if(app->signal_count >= IR_ANALYZER_MAX_SIGNALS) return;

    tmp.seen_count = 1;
    app->signals[app->signal_count] = tmp;
    app->signal_count++;

    AppEvent event = {.type = EventTypeIrSignal};
    furi_message_queue_put(app->event_queue, &event, 0);
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_green_10);
}

// ── Callback input ────────────────────────────────────────────────────────────
static void input_callback(InputEvent* input_event, void* ctx) {
    IrAnalyzerApp* app = ctx;
    AppEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// ── Draw : vue LIVE ───────────────────────────────────────────────────────────
static void draw_live(Canvas* canvas, IrAnalyzerApp* app) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "En attente d'un signal...");
    canvas_draw_str(canvas, 2, 36, "Pointe une telecommande");
    canvas_draw_str(canvas, 2, 48, "vers le port IR");

    char buf[32];
    snprintf(buf, sizeof(buf), "Captes: %lu", app->signal_count);
    canvas_draw_str(canvas, 2, 62, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 90, 62, "[OK:liste]");
}

// ── Draw : vue LISTE ──────────────────────────────────────────────────────────
static void draw_list(Canvas* canvas, IrAnalyzerApp* app) {
    if(app->signal_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 35, "Aucun signal capte");
        return;
    }

    const int visible = 4;
    int32_t start = app->list_index - visible + 1;
    if(start < 0) start = 0;

    for(int i = 0; i < visible; i++) {
        int32_t idx = start + i;
        if((uint32_t)idx >= app->signal_count) break;

        IrAnalyzerSignal* sig = &app->signals[idx];
        char buf[40];
        int y = 17 + i * 12;

        bool selected = (idx == app->list_index);
        if(selected) {
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }

        if(sig->is_raw) {
            snprintf(buf, sizeof(buf), "%ld. RAW (%lu t) x%lu", idx + 1, sig->raw_count, sig->seen_count);
        } else {
            snprintf(buf, sizeof(buf), "%ld. %s 0x%04lX x%lu", idx + 1, sig->protocol, sig->command, sig->seen_count);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, y, buf);

        if(selected) canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 62, "[OK:detail] [Back:live]");
}

// ── Draw : vue DETAIL ─────────────────────────────────────────────────────────
static void draw_detail(Canvas* canvas, IrAnalyzerApp* app) {
    if(app->signal_count == 0) return;
    IrAnalyzerSignal* sig = &app->signals[app->list_index];
    char buf[48];
    int y = 24;

    canvas_set_font(canvas, FontSecondary);

    snprintf(buf, sizeof(buf), "Proto: %s", sig->protocol);
    canvas_draw_str(canvas, 2, y, buf); y += 11;

    if(!sig->is_raw) {
        snprintf(buf, sizeof(buf), "Addr:  0x%04lX", sig->address);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        snprintf(buf, sizeof(buf), "Cmd:   0x%04lX", sig->command);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        snprintf(buf, sizeof(buf), "Repeat: %s", sig->repeat ? "Oui" : "Non");
        canvas_draw_str(canvas, 2, y, buf); y += 11;
    } else {
        snprintf(buf, sizeof(buf), "Timings: %lu", sig->raw_count);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        if(sig->raw_count > 0) {
            snprintf(buf, sizeof(buf), "T1: %luus", sig->raw_timings[0]);
            canvas_draw_str(canvas, 2, y, buf); y += 11;
        }
    }
    snprintf(buf, sizeof(buf), "Vus: %lu fois", sig->seen_count);
    canvas_draw_str(canvas, 2, y, buf);

    canvas_draw_str(canvas, 0, 62, "[OK:save] [Back:liste]");
}

// ── Draw principal ────────────────────────────────────────────────────────────
static void draw_callback(Canvas* canvas, void* ctx) {
    IrAnalyzerApp* app = ctx;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "IR Analyzer");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    switch(app->view) {
    case ViewLive:   draw_live(canvas, app);   break;
    case ViewList:   draw_list(canvas, app);   break;
    case ViewDetail: draw_detail(canvas, app); break;
    }
}

// ── Gestion des inputs ────────────────────────────────────────────────────────
static void handle_input(IrAnalyzerApp* app, InputEvent* e) {
    if(e->type == InputTypeShort) {
        switch(app->view) {

        case ViewLive:
            if(e->key == InputKeyOk) {
                app->view = ViewList;
                if(app->signal_count > 0)
                    app->list_index = (int32_t)app->signal_count - 1;
            }
            if(e->key == InputKeyBack) app->running = false;
            break;

        case ViewList:
            if(e->key == InputKeyUp && app->list_index > 0)
                app->list_index--;
            if(e->key == InputKeyDown && (uint32_t)(app->list_index + 1) < app->signal_count)
                app->list_index++;
            if(e->key == InputKeyOk && app->signal_count > 0)
                app->view = ViewDetail;
            if(e->key == InputKeyBack)
                app->view = ViewLive;
            break;

        case ViewDetail:
            if(e->key == InputKeyOk && app->signal_count > 0) {
                bool saved = save_signal(&app->signals[app->list_index], (uint32_t)app->list_index);
                if(app->notifications)
                    notification_message(app->notifications,
                        saved ? &sequence_blink_green_10 : &sequence_blink_red_10);
            }
            if(e->key == InputKeyBack)
                app->view = ViewList;
            break;
        }
    }

    // Long press Back depuis liste = effacer tout
    if(e->type == InputTypeLong && e->key == InputKeyBack) {
        if(app->view == ViewList) {
            app->signal_count = 0;
            app->list_index   = 0;
            app->view         = ViewLive;
        }
    }
}

// ── Point d'entrée ────────────────────────────────────────────────────────────
int32_t ir_analyzer_app(void* p) {
    UNUSED(p);

    IrAnalyzerApp* app = malloc(sizeof(IrAnalyzerApp));
    furi_check(app);
    memset(app, 0, sizeof(IrAnalyzerApp));

    app->event_queue   = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ir_signal_callback, app);
    infrared_worker_rx_start(app->ir_worker);

    app->running     = true;
    app->view        = ViewLive;
    app->list_index  = 0;

    AppEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                handle_input(app, &event.input);
            }
            view_port_update(app->view_port);
        }
    }

    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}
