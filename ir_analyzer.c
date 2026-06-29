#include "ir_analyzer.h"
#include <infrared.h>
#include <infrared_worker.h>
#include <storage/storage.h>

typedef enum {
    EventInput,
    EventSignal,
} EventType;

typedef struct {
    EventType  type;
    InputEvent input;
} AppEvent;

// ── Sauvegarde ────────────────────────────────────────────────────────────────
static void save_signal(IrSignal* sig, uint32_t idx) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(s, "/ext/infrared");
    storage_common_mkdir(s, IR_SAVE_PATH);

    char path[128];
    snprintf(path, sizeof(path), IR_SAVE_PATH "/sig%u.ir", (unsigned)idx);

    File* f = storage_file_alloc(s);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        int len;
        len = snprintf(buf, sizeof(buf), "Filetype: IR signals file\nVersion: 1\n#\n");
        storage_file_write(f, buf, (uint16_t)len);

        if(sig->is_raw) {
            len = snprintf(buf, sizeof(buf),
                "name: RAW%u\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata:", (unsigned)idx);
            storage_file_write(f, buf, (uint16_t)len);
            for(uint32_t i = 0; i < sig->timing_count; i++) {
                len = snprintf(buf, sizeof(buf), " %u", (unsigned)sig->timings[i]);
                storage_file_write(f, buf, (uint16_t)len);
            }
            storage_file_write(f, "\n", 1);
        } else {
            storage_file_write(f, sig->info, (uint16_t)strlen(sig->info));
            storage_file_write(f, "\n", 1);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// ── Callback IR ───────────────────────────────────────────────────────────────
static void ir_callback(void* ctx, InfraredWorkerSignal* received) {
    IrApp* app = ctx;
    if(!app || app->signal_count >= IR_MAX_SIGNALS) return;

    IrSignal tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.seen_count = 1;

    if(infrared_worker_signal_is_decoded(received)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received);
        tmp.is_raw = false;
        snprintf(tmp.info, sizeof(tmp.info), "%s A:0x%04X C:0x%04X%s",
            infrared_get_protocol_name(msg->protocol),
            (unsigned)msg->address,
            (unsigned)msg->command,
            msg->repeat ? " R" : "");
    } else {
        const uint32_t* t;
        size_t cnt;
        infrared_worker_get_raw_signal(received, &t, &cnt);
        tmp.is_raw = true;
        tmp.timing_count = cnt < IR_MAX_TIMINGS ? (uint32_t)cnt : IR_MAX_TIMINGS;
        memcpy(tmp.timings, t, tmp.timing_count * sizeof(uint32_t));
        snprintf(tmp.info, sizeof(tmp.info), "RAW %u timings", (unsigned)tmp.timing_count);
    }

    // Deduplication
    for(uint32_t i = 0; i < app->signal_count; i++) {
        if(strcmp(app->signals[i].info, tmp.info) == 0) {
            app->signals[i].seen_count++;
            AppEvent ev = {.type = EventSignal};
            furi_message_queue_put(app->queue, &ev, 0);
            if(app->notifications)
                notification_message(app->notifications, &sequence_blink_yellow_10);
            return;
        }
    }

    app->signals[app->signal_count++] = tmp;
    AppEvent ev = {.type = EventSignal};
    furi_message_queue_put(app->queue, &ev, 0);
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_green_10);
}

// ── Input callback ────────────────────────────────────────────────────────────
static void input_callback(InputEvent* e, void* ctx) {
    IrApp* app = ctx;
    AppEvent ev = {.type = EventInput, .input = *e};
    furi_message_queue_put(app->queue, &ev, FuriWaitForever);
}

// ── Draw ──────────────────────────────────────────────────────────────────────
static void draw_live(Canvas* canvas, IrApp* app) {
    canvas_set_font(canvas, FontSecondary);
    if(app->signal_count == 0) {
        canvas_draw_str(canvas, 2, 26, "En attente d'un signal...");
        canvas_draw_str(canvas, 2, 38, "Pointe une telecommande");
        canvas_draw_str(canvas, 2, 50, "vers le port IR");
    } else {
        IrSignal* s = &app->signals[app->signal_count - 1];
        canvas_draw_str(canvas, 2, 26, s->info);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "Total: %u  [OK:liste]", (unsigned)app->signal_count);
    canvas_draw_str(canvas, 0, 62, buf);
}

static void draw_list(Canvas* canvas, IrApp* app) {
    canvas_set_font(canvas, FontSecondary);
    if(app->signal_count == 0) {
        canvas_draw_str(canvas, 2, 35, "Aucun signal");
        canvas_draw_str(canvas, 0, 62, "[Back:live]");
        return;
    }
    int32_t start = app->list_index - 3;
    if(start < 0) start = 0;
    for(int32_t i = 0; i < 4; i++) {
        int32_t idx = start + i;
        if((uint32_t)idx >= app->signal_count) break;
        IrSignal* s = &app->signals[idx];
        char buf[40];
        uint8_t y = (uint8_t)(17 + i * 12);
        bool sel = (idx == app->list_index);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        snprintf(buf, sizeof(buf), "%d.%.22s x%u", (int)(idx+1), s->info, (unsigned)s->seen_count);
        canvas_draw_str(canvas, 2, y, buf);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 0, 62, "[OK:detail][BL:reset]");
}

static void draw_detail(Canvas* canvas, IrApp* app) {
    if(!app->signal_count || app->list_index < 0) return;
    IrSignal* s = &app->signals[(uint32_t)app->list_index];
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, s->info);
    char buf[40];
    snprintf(buf, sizeof(buf), "Vus: %u fois", (unsigned)s->seen_count);
    canvas_draw_str(canvas, 2, 36, buf);
    if(s->is_raw) {
        snprintf(buf, sizeof(buf), "T1:%uus T2:%uus",
            s->timing_count > 0 ? (unsigned)s->timings[0] : 0,
            s->timing_count > 1 ? (unsigned)s->timings[1] : 0);
        canvas_draw_str(canvas, 2, 50, buf);
    }
    canvas_draw_str(canvas, 0, 62, "[OK:save][Back:liste]");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    IrApp* app = ctx;
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

// ── Input ─────────────────────────────────────────────────────────────────────
static void handle_input(IrApp* app, InputEvent* e) {
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
            else if(e->key == InputKeyDown && (uint32_t)(app->list_index+1) < app->signal_count)
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

// ── Main ──────────────────────────────────────────────────────────────────────
int32_t ir_analyzer_app(void* p) {
    UNUSED(p);

    IrApp* app = malloc(sizeof(IrApp));
    furi_check(app);
    memset(app, 0, sizeof(IrApp));

    app->queue         = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InfraredWorker* worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(worker, ir_callback, app);
    infrared_worker_rx_start(worker);

    app->running    = true;
    app->view       = ViewLive;
    app->list_index = 0;

    AppEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventInput)
                handle_input(app, &event.input);
            view_port_update(app->view_port);
        }
    }

    infrared_worker_rx_stop(worker);
    infrared_worker_free(worker);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
