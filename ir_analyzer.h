#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define IR_ANALYZER_MAX_SIGNALS 32
#define IR_ANALYZER_MAX_RAW     512
#define IR_SAVE_PATH            "/ext/infrared/ir_analyzer"

typedef enum {
    ViewLive,
    ViewList,
    ViewDetail,
} AppView;

typedef struct {
    char     protocol[32];
    uint32_t address;
    uint32_t command;
    bool     repeat;
    bool     is_raw;
    uint32_t raw_count;
    uint32_t raw_timings[IR_ANALYZER_MAX_RAW];
    uint32_t seen_count;
} IrAnalyzerSignal;

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    NotificationApp*  notifications;
    InfraredWorker*   ir_worker;

    IrAnalyzerSignal  signals[IR_ANALYZER_MAX_SIGNALS];
    uint32_t          signal_count;
    bool              running;

    AppView           view;
    int32_t           list_index;
    uint32_t          last_signal_count;
} IrAnalyzerApp;
