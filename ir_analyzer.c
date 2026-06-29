#include "ir_analyzer.h"

typedef enum {
    EventTypeInput,
    EventTypeIrSignal,
} EventType;

typedef struct {
    EventType type;
    union {
        InputEvent input;
    };
} AppEvent;

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
        && a->command == b->command;
}

static void save_signal(IrAnalyzerSignal* sig, uint32_t index) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/infrared");
    storage_common_mkdir(storage, IR_SAVE_PATH);

    char path[128];
    char buf[256];
    int len;

    if(sig->is_raw) {
        snprintf(path, sizeof(path), IR_SAVE_PATH "/sig%u_RAW.ir", (unsigned)index);
    } else {
        snprintf(path, sizeof(path), IR_SAVE_PATH "/sig%u_%s.ir", (unsigned)index, sig->protocol);
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        len = snprintf(buf, sizeof(buf), "Filetype: IR signals file\nVersion: 1\n#\n");
        storage_file_write(file, buf, (uint16_t)len);
        if(sig->is_raw) {
            len = snprintf(buf, sizeof(buf),
                "name: RAW%u\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata:", (unsigned)index);
            storage_file_write(file, buf, (uint16_t)len);
            for(uint32_t i = 0; i < sig->raw_count; i++) {
                len = snprintf(buf, sizeof(buf), " %u", (unsigned)sig->raw_timings[i]);
                storage_file_write(file, buf, (uint16_t)len);
            }
            storage_file_write(file, "\n", 1);
        } else {
            len = snprintf(buf, sizeof(buf),
                "name: %s%04X\ntype: parsed\nprotocol: %s\naddress: 0x%04X\ncommand: 0x%04X\n",
                sig->protocol, (unsigned)sig->command,
                sig->protocol, (unsigned)sig->address, (unsigned)sig->command);
            storage_file_write(file, buf, (uint16_t)len);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

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
        tmp.raw_count = (count < IR_ANALYZER_MAX_RAW) ? (uint32_t)count : IR_ANALYZER_MAX_RAW;
        memcpy(tmp.raw_timings, timings, tmp.raw_count * sizeof(uint32_t));
    }

    for(uint32_t i = 0; i < app->signal_count; i++) {
        if(signals_match(&app->signals[i], &tmp)) {
            app->signals[i].seen_count++;
            AppEvent ev = {.type = EventTypeIrSignal};
            furi_message_queue_put(app->event_queue, &ev, 0);
            if(app->notifications)
                notification_message(app->notifications, &sequence_blink_yellow_10);
            return;
        }
    }

    if(app->signal_count >= IR_ANALYZER_MAX_SIGNALS) return;
    tmp.seen_count = 1;
    app->signals[app->signal_count++] = tmp;

    AppEvent ev = {.type = EventTypeIrSignal};
    furi_message_queue_put(app->event_queue, &ev, 0);
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_green_10);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    IrAnalyzerApp* app = ctx;
    AppEvent ev = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &ev, FuriWaitForever);
}

static void draw_live(Canvas* canvas, IrAnalyzerApp* app) {
    char buf[32];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "En attente d'un signal...");
    canvas_draw_str(canvas, 2, 36, "Pointe une telecommande");
    canvas_draw_str(canvas, 2, 48, "vers le port IR");
    snprintf(buf, sizeof(buf), "Captes: %u", (unsigned)app->signal_count);
    canvas_draw_str(canvas, 2, 62, buf);
}

static void draw_list(Canvas* canvas, IrAnalyzerApp* app) {
    canvas_set_font(canvas, FontSecondary);
    if(app->signal_count == 0) {
        canvas_draw_str(canvas, 2, 35, "Aucun signal capte");
        canvas_draw_str(canvas, 2, 62, "[Back:live]");
        return;
    }

    int32_t start = app->list_index - 3;
    if(start < 0) start = 0;

    for(int32_t i = 0; i < 4; i++) {
        int32_t idx = start + i;
        if((uint32_t)idx >= app->signal_count) break;

        IrAnalyzerSignal* sig = &app->signals[idx];
        char buf[40];
        uint8_t y = (uint8_t)(17 + i * 12);
        bool sel = (idx == app->list_index);

        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }

        if(sig->is_raw) {
            snprintf(buf, sizeof(buf), "%d.RAW(%u)x%u", (int)(idx+1), (unsigned)sig->raw_count, (unsigned)sig->seen_count);
        } else {
            snprintf(buf, sizeof(buf), "%d.%s %04X x%u", (int)(idx+1), sig->protocol, (unsigned)sig->command, (unsigned)sig->seen_count);
        }
        canvas_draw_str(canvas, 2, y, buf);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 0, 62, "[OK:detail][Back:live]");
}

static void draw_detail(Canvas* canvas, IrAnalyzerApp* app) {
    if(app->signal_count == 0 || app->list_index < 0) return;
    IrAnalyzerSignal* sig = &app->signals[(uint32_t)app->list_index];
    char buf[48];
    uint8_t y = 24;

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Proto: %s", sig->protocol);
    canvas_draw_str(canvas, 2, y, buf); y += 11;

    if(!sig->is_raw) {
        snprintf(buf, sizeof(buf), "Addr: 0x%04X", (unsigned)sig->address);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        snprintf(buf, sizeof(buf), "Cmd:  0x%04X", (unsigned)sig->command);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        snprintf(buf, sizeof(buf), "Repeat: %s", sig->repeat ? "Oui" : "Non");
        canvas_draw_str(canvas, 2, y, buf); y += 11;
    } else {
        snprintf(buf, sizeof(buf), "Timings: %u", (unsigned)sig->raw_count);
        canvas_draw_str(canvas, 2, y, buf); y += 11;
        if(sig->raw_count > 0) {
            snprintf(buf, sizeof(buf), "T1: %uus", (unsigned)sig->raw_timings[0]);
            canvas_draw_str(canvas, 2, y, buf); y += 11;
        }
    }
    snprintf(buf, sizeof(buf), "Vus: %u fois", (unsigned)sig->seen_count);
    canvas_draw_str(canvas, 2, y, buf);
    canvas_draw_str(canvas, 0, 62, "[OK:save][Back:liste]");
}

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

static void handle_input(IrAnalyzerApp* app, InputEvent* e) {
    if(e->type == InputTypeShort) {
        switch(app->view) {
        case ViewLive:
            if(e->key == InputKeyOk) {
                app->view = ViewList;
                if(app->signal_count > 0)
                    app->list_index = (int32_t)app->signal_count - 1;
            } else if(e->key == InputKeyBack) {
                app->running = false;
            }
            break;
        case ViewList:
            if(e->key == InputKeyUp && app->list_index > 0)
                app->list_index--;
            else if(e->key == InputKeyDown && (uint32_t)(app->list_index + 1) < app->signal_count)
                app->list_index++;
            else if(e->key == InputKeyOk && app->signal_count > 0)
                app->view = ViewDetail;
            else if(e->key == InputKeyBack)
                app->view = ViewLive;
            break;
        case ViewDetail:
            if(e->key == InputKeyOk && app->signal_count > 0) {
                save_signal(&app->signals[(uint32_t)app->list_index], (uint32_t)app->list_index);
                if(app->notifications)
                    notification_message(app->notifications, &sequence_blink_green_10);
            } else if(e->key == InputKeyBack) {
                app->view = ViewList;
            }
            break;
        }
    } else if(e->type == InputTypeLong && e->key == InputKeyBack && app->view == ViewList) {
        app->signal_count = 0;
        app->list_index   = 0;
        app->view         = ViewLive;
    }
}

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

    app->running    = true;
    app->view       = ViewLive;
    app->list_index = 0;

    AppEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeInput)
                handle_input(app, &event.input);
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
