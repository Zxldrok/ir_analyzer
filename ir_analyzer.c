#include "ir_analyzer.h"
#include <storage/storage.h>

#define HEADER_Y   10
#define HEADER_LINE 12
#define CONTENT_TOP 14
#define CONTENT_BOT 48
#define FOOTER_LINE 50
#define FOOTER_Y   60

static void save_signal_to_file(Storage* s, IrSignal* sig, uint32_t idx) {
    char path[128];
    snprintf(path, sizeof(path), IR_SAVE_PATH "/sig%u.ir", (unsigned)idx);
    File* f = storage_file_alloc(s);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        int len;
        len = snprintf(buf, sizeof(buf), "Filetype: IR signals file\nVersion: 1\n#\n");
        storage_file_write(f, buf, (uint16_t)len);
        if(sig->is_raw) {
            uint32_t freq = sig->frequency ? sig->frequency : 38000;
            float duty = sig->duty_cycle ? sig->duty_cycle : 0.33f;
            len = snprintf(buf, sizeof(buf),
                "name: RAW%u\ntype: raw\nfrequency: %lu\nduty_cycle: %.2f\ndata:",
                (unsigned)idx, (unsigned long)freq, (double)duty);
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
}

static void save_signal(IrApp* app, uint32_t idx) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(s, "/ext/infrared");
    storage_common_mkdir(s, IR_SAVE_PATH);
    save_signal_to_file(s, &app->signals[idx], idx);
    furi_record_close(RECORD_STORAGE);
}

static void save_all_signals(IrApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(s, "/ext/infrared");
    storage_common_mkdir(s, IR_SAVE_PATH);
    for(uint32_t i = 0; i < app->signal_count; i++)
        save_signal_to_file(s, &app->signals[i], i);
    furi_record_close(RECORD_STORAGE);
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_green_10);
}

static void delete_signal(IrApp* app, int32_t idx) {
    if(app->signal_count == 0 || idx < 0 || (uint32_t)idx >= app->signal_count) return;
    for(uint32_t i = (uint32_t)idx; i < app->signal_count - 1; i++)
        app->signals[i] = app->signals[i + 1];
    app->signal_count--;
    if(app->last_sig_idx == idx) app->last_sig_idx = -1;
    else if(app->last_sig_idx > idx) app->last_sig_idx--;
    if(app->tx_sig_idx == idx) app->tx_sig_idx = -1;
    else if(app->tx_sig_idx > idx) app->tx_sig_idx--;
    if(app->list_index >= (int32_t)app->signal_count)
        app->list_index = (int32_t)app->signal_count - 1;
    if(app->list_index < 0) app->list_index = 0;
    if(app->signal_count == 0) app->view = ViewLive;
}

static void tx_sent_callback(void* context) {
    IrApp* app = context;
    AppEvent ev = {.type = EventTxComplete};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void setup_tx(IrApp* app, int32_t idx) {
    if(idx < 0 || (uint32_t)idx >= app->signal_count) return;
    IrSignal* sig = &app->signals[idx];
    if(sig->is_raw)
        infrared_worker_set_raw_signal(app->worker, sig->timings, sig->timing_count,
            sig->frequency ? sig->frequency : 38000,
            sig->duty_cycle ? sig->duty_cycle : 0.33f);
    else
        infrared_worker_set_decoded_signal(app->worker, &sig->message);
}

static void start_tx(IrApp* app, int32_t idx) {
    if(app->tx_state != TxIdle) return;
    if(idx < 0 || (uint32_t)idx >= app->signal_count) return;
    app->tx_sig_idx = idx;
    app->tx_state = TxActive;
    infrared_worker_rx_stop(app->worker);
    setup_tx(app, idx);
    infrared_worker_tx_set_get_signal_callback(
        app->worker, infrared_worker_tx_get_signal_steady_callback, app);
    infrared_worker_tx_set_signal_sent_callback(app->worker, tx_sent_callback, app);
    infrared_worker_tx_start(app->worker);
    app->view = ViewTransmit;
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_start_blue);
}

static void stop_tx(IrApp* app) {
    if(app->tx_state == TxIdle) return;
    infrared_worker_tx_stop(app->worker);
    infrared_worker_rx_start(app->worker);
    if(app->notifications)
        notification_message(app->notifications, &sequence_blink_stop);
    app->tx_state = TxIdle;
    app->tx_remaining = 0;
    app->tx_completed = 0;
    app->view = ViewDetail;
}

static void ir_callback(void* ctx, InfraredWorkerSignal* received) {
    IrApp* app = ctx;
    if(!app || app->signal_count >= IR_MAX_SIGNALS) return;

    IrSignal tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.seen_count = 1;
    tmp.frequency = 38000;
    tmp.duty_cycle = 0.33f;

    if(infrared_worker_signal_is_decoded(received)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received);
        tmp.is_raw = false;
        tmp.message = *msg;
        snprintf(tmp.info, sizeof(tmp.info), "%s A:0x%04lX C:0x%04lX%s",
            infrared_get_protocol_name(msg->protocol),
            (unsigned long)msg->address, (unsigned long)msg->command,
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

    int32_t matched = -1;
    for(uint32_t i = 0; i < app->signal_count; i++)
        if(strcmp(app->signals[i].info, tmp.info) == 0) { matched = (int32_t)i; break; }

    if(matched >= 0) {
        app->signals[matched].seen_count++;
        app->last_sig_idx = matched;
    } else {
        app->signals[app->signal_count] = tmp;
        app->last_sig_idx = (int32_t)app->signal_count;
        app->signal_count++;
    }
    AppEvent ev = {.type = matched >= 0 ? EventNotifyYellow : EventNotifyGreen};
    furi_message_queue_put(app->queue, &ev, 0);
    ev.type = EventSignal;
    furi_message_queue_put(app->queue, &ev, 0);
}

static void input_callback(InputEvent* e, void* ctx) {
    IrApp* app = ctx;
    AppEvent ev = {.type = EventInput, .input = *e};
    furi_message_queue_put(app->queue, &ev, 0);
}

// ── Drawing ───────────────────────────────────────────────────────────────

static void draw_header(Canvas* canvas, const char* right) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, HEADER_Y, "IR Analyzer");
    if(right) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, HEADER_Y, AlignRight, AlignBottom, right);
    }
    canvas_draw_line(canvas, 0, HEADER_LINE, 127, HEADER_LINE);
}

static void draw_footer(Canvas* canvas, const char* left, const char* right) {
    canvas_draw_line(canvas, 0, FOOTER_LINE, 127, FOOTER_LINE);
    canvas_set_font(canvas, FontSecondary);
    if(left) canvas_draw_str(canvas, 2, FOOTER_Y, left);
    if(right)
        canvas_draw_str_aligned(canvas, 126, FOOTER_Y, AlignRight, AlignBottom, right);
}

static void draw_live(Canvas* canvas, IrApp* app) {
    char buf[64];
    uint32_t elapsed = (furi_get_tick() - app->session_start) / 1000;
    snprintf(buf, sizeof(buf), "%um%us | %u sig",
        (unsigned)(elapsed / 60), (unsigned)(elapsed % 60), (unsigned)app->signal_count);

    if(app->signal_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "En attente...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter,
            "Pointe la telecommande");
    } else if(app->last_sig_idx >= 0) {
        IrSignal* s = &app->signals[(uint32_t)app->last_sig_idx];
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, s->info);
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf + 16, 48, "Vu %u fois", (unsigned)s->seen_count);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, buf + 16);
        if(s->is_raw) {
            uint32_t total_us = 0;
            for(uint32_t i = 0; i < s->timing_count; i++) total_us += s->timings[i];
            snprintf(buf + 16, 48, "%u timings, %u ms",
                (unsigned)s->timing_count, (unsigned)(total_us / 1000));
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, buf + 16);
        }
    }

    draw_footer(canvas, "[OK:list]", buf);
}

static void draw_scrollbar(Canvas* canvas, uint32_t pos, uint32_t total) {
    if(total <= 4) return;
    uint32_t bar_h = CONTENT_BOT - CONTENT_TOP;
    uint32_t thumb_h = (bar_h * 4) / total;
    if(thumb_h < 4) thumb_h = 4;
    uint32_t thumb_y = CONTENT_TOP + (pos * (bar_h - thumb_h)) / (total - 1);
    canvas_draw_frame(canvas, 125, CONTENT_TOP, 2, bar_h);
    canvas_draw_box(canvas, 125, (int32_t)thumb_y, 2, thumb_h);
}

static void draw_list(Canvas* canvas, IrApp* app) {
    if(app->signal_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "Aucun signal");
        draw_footer(canvas, "[Back:live]", NULL);
        return;
    }

    char buf[32];
    uint32_t total_pages = (app->signal_count + 3) / 4;
    uint32_t cur_page = (uint32_t)app->list_index / 4 + 1;
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)cur_page, (unsigned)total_pages);
    draw_header(canvas, buf);

    int32_t start = (app->list_index / 4) * 4;
    for(int32_t i = 0; i < 4; i++) {
        int32_t idx = start + i;
        if((uint32_t)idx >= app->signal_count) break;
        IrSignal* s = &app->signals[idx];
        uint8_t y = (uint8_t)(17 + i * 11);
        bool sel = (idx == app->list_index);

        if(sel) {
            canvas_draw_rbox(canvas, 0, y - 8, 124, 10, 2);
            canvas_set_color(canvas, ColorWhite);
        }

        snprintf(buf, sizeof(buf), "%u.", (unsigned)(idx + 1));
        canvas_draw_str(canvas, 2, y, buf);
        uint8_t x = canvas_string_width(canvas, buf) + 3;
        const char* star = (idx == app->last_sig_idx) ? "*" : "";
        snprintf(buf, sizeof(buf), "%s%.20s", star, s->info);
        canvas_draw_str(canvas, x, y, buf);

        snprintf(buf, sizeof(buf), "x%u", (unsigned)s->seen_count);
        uint16_t w = canvas_string_width(canvas, buf);
        canvas_draw_str(canvas, 124 - w - 2, y, buf);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    draw_scrollbar(canvas, (uint32_t)app->list_index, app->signal_count);
    draw_footer(canvas, "[OK:detail]", "[LgOK:all]");
}

static void draw_waveform(Canvas* canvas, IrSignal* sig, int32_t x, int32_t w) {
    if(sig->timing_count == 0) return;
    uint32_t max_t = 0;
    for(uint32_t i = 0; i < sig->timing_count; i++)
        if(sig->timings[i] > max_t) max_t = sig->timings[i];
    if(max_t == 0) max_t = 1;

    uint32_t n = sig->timing_count < 100 ? sig->timing_count : 100;
    uint32_t step = sig->timing_count / n;
    if(step < 1) step = 1;

    int32_t by = FOOTER_LINE - 2;
    canvas_draw_frame(canvas, x, by - 9, w, 10);
    for(uint32_t i = 0, px = 0; i < sig->timing_count && (int32_t)px < w; i += step, px++) {
        uint32_t bar_h = (sig->timings[i] * 8) / max_t;
        if(bar_h < 1) bar_h = 1;
        if(bar_h > 8) bar_h = 8;
        if(i % 2 == 0)
            canvas_draw_box(canvas, x + (int32_t)px, by - (int32_t)bar_h, 1, (int32_t)bar_h);
    }
}

static void draw_detail(Canvas* canvas, IrApp* app) {
    if(!app->signal_count || app->list_index < 0) return;
    IrSignal* s = &app->signals[(uint32_t)app->list_index];

    char buf[64];
    char tag[16];
    snprintf(tag, sizeof(tag), "#%d", (int)(app->list_index + 1));
    draw_header(canvas, tag);

    bool is_last = (app->list_index == app->last_sig_idx);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%s%s", is_last ? "* " : "", s->info);
    canvas_draw_str(canvas, 2, 22, buf);

    if(s->is_raw) {
        uint32_t total_us = 0, min_t = s->timings[0], max_t = 0;
        for(uint32_t i = 0; i < s->timing_count; i++) {
            total_us += s->timings[i];
            if(s->timings[i] < min_t) min_t = s->timings[i];
            if(s->timings[i] > max_t) max_t = s->timings[i];
        }
        snprintf(buf, sizeof(buf), "Timings: %u", (unsigned)s->timing_count);
        canvas_draw_str(canvas, 2, 33, buf);
        snprintf(buf, sizeof(buf), "Dur: %u ms", (unsigned)(total_us / 1000));
        canvas_draw_str(canvas, 68, 33, buf);
        uint32_t freq = s->frequency ? s->frequency : 38000;
        snprintf(buf, sizeof(buf), "%lu Hz | %u us min", (unsigned long)freq, (unsigned)min_t);
        canvas_draw_str(canvas, 2, 43, buf);
        draw_waveform(canvas, s, 2, 124);
    } else {
        snprintf(buf, sizeof(buf), "Addr: 0x%04lX", (unsigned long)s->message.address);
        canvas_draw_str(canvas, 2, 33, buf);
        snprintf(buf, sizeof(buf), "Cmd:  0x%04lX", (unsigned long)s->message.command);
        canvas_draw_str(canvas, 68, 33, buf);
        snprintf(buf, sizeof(buf), "%s  %lu Hz",
            s->message.repeat ? "Repeat" : "Normal",
            (unsigned long)infrared_get_protocol_frequency(s->message.protocol));
        canvas_draw_str(canvas, 2, 43, buf);
    }

    char lft[24], rgt[24];
    snprintf(lft, sizeof(lft), "[OK:save]");
    snprintf(rgt, sizeof(rgt), "[<>:x%u]", (unsigned)app->turbo_repeat);
    draw_footer(canvas, lft, rgt);
}

static void draw_transmit(Canvas* canvas, IrApp* app) {
    draw_header(canvas, app->tx_remaining > 0 ? "BURST" : "TX");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "Envoi en cours...");

    canvas_draw_rframe(canvas, 30, 28, 68, 13, 3);
    uint32_t fill_w = (app->tx_completed * 64) / (app->turbo_repeat > 0 ? app->turbo_repeat : 1);
    if(fill_w > 0) canvas_draw_rbox(canvas, 32, 30, fill_w, 9, 2);

    char buf[16];
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)app->tx_completed, (unsigned)app->turbo_repeat);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, buf);
    canvas_set_color(canvas, ColorBlack);

    if(app->tx_sig_idx >= 0 && (uint32_t)app->tx_sig_idx < app->signal_count) {
        IrSignal* s = &app->signals[(uint32_t)app->tx_sig_idx];
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, s->info);
    }

    draw_footer(canvas, "[Back:stop]", NULL);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    IrApp* app = ctx;
    if(!app) return;
    canvas_clear(canvas);
    switch(app->view) {
    case ViewLive:
        draw_header(canvas, app->signal_count > 0 ? "LIVE" : NULL);
        draw_live(canvas, app);
        break;
    case ViewList:
        draw_list(canvas, app);
        break;
    case ViewDetail:
        draw_detail(canvas, app);
        break;
    case ViewTransmit:
        draw_transmit(canvas, app);
        break;
    }
}

// ── Input ─────────────────────────────────────────────────────────────────

static void handle_input(IrApp* app, InputEvent* e) {
    if(app->view == ViewTransmit) {
        if(e->type == InputTypeShort && e->key == InputKeyBack) stop_tx(app);
        return;
    }

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
            else if(e->key == InputKeyLeft) {
                app->list_index -= 4;
                if(app->list_index < 0) app->list_index = 0;
            } else if(e->key == InputKeyRight) {
                app->list_index += 4;
                if((uint32_t)app->list_index >= app->signal_count)
                    app->list_index = (int32_t)app->signal_count - 1;
            } else if(e->key == InputKeyOk && app->signal_count > 0)
                app->view = ViewDetail;
            else if(e->key == InputKeyBack)
                app->view = ViewLive;
            break;
        case ViewDetail:
            if(e->key == InputKeyLeft) {
                if(app->turbo_repeat > 1) app->turbo_repeat--;
            } else if(e->key == InputKeyRight) {
                if(app->turbo_repeat < TURBO_MAX) app->turbo_repeat++;
            } else if(e->key == InputKeyOk && app->signal_count > 0) {
                save_signal(app, (uint32_t)app->list_index);
                if(app->notifications)
                    notification_message(app->notifications, &sequence_blink_green_10);
            } else if(e->key == InputKeyBack) {
                app->view = ViewList;
            }
            break;
        default:
            break;
        }
    } else if(e->type == InputTypeLong) {
        switch(app->view) {
        case ViewLive:
            break;
        case ViewList:
            if(e->key == InputKeyOk && app->signal_count > 0)
                save_all_signals(app);
            else if(e->key == InputKeyBack) {
                app->signal_count = 0;
                app->list_index = 0;
                app->last_sig_idx = -1;
                app->view = ViewLive;
            }
            break;
        case ViewDetail:
            if(e->key == InputKeyOk && app->signal_count > 0) {
                app->tx_remaining = app->turbo_repeat;
                app->tx_completed = 0;
                start_tx(app, app->list_index);
            } else if(e->key == InputKeyBack) {
                delete_signal(app, app->list_index);
                if(app->signal_count > 0)
                    app->view = ViewList;
            }
            break;
        default:
            break;
        }
    }
}

// ── Main ──────────────────────────────────────────────────────────────────

int32_t ir_analyzer_app(void* p) {
    UNUSED(p);

    IrApp* app = malloc(sizeof(IrApp));
    furi_check(app);
    memset(app, 0, sizeof(IrApp));

    app->queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(app->worker, ir_callback, app);
    infrared_worker_rx_start(app->worker);

    app->running = true;
    app->view = ViewLive;
    app->list_index = 0;
    app->turbo_repeat = 1;
    app->last_sig_idx = -1;
    app->tx_sig_idx = -1;
    app->tx_state = TxIdle;
    app->tx_completed = 0;
    app->session_start = furi_get_tick();

    AppEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTxComplete) {
                if(app->tx_state == TxActive && app->tx_remaining > 1) {
                    app->tx_remaining--;
                    app->tx_completed++;
                    setup_tx(app, app->tx_sig_idx);
                    infrared_worker_tx_start(app->worker);
                } else {
                    app->tx_state = TxIdle;
                    app->tx_remaining = 0;
                    app->tx_completed = 0;
                    stop_tx(app);
                }
            } else if(event.type == EventNotifyGreen) {
                notification_message(app->notifications, &sequence_blink_green_10);
            } else if(event.type == EventNotifyYellow) {
                notification_message(app->notifications, &sequence_blink_yellow_10);
            } else if(event.type == EventInput) {
                handle_input(app, &event.input);
            }
            view_port_update(app->view_port);
        }
    }

    infrared_worker_rx_stop(app->worker);
    infrared_worker_free(app->worker);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
