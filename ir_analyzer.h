#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <notification/notification_messages.h>

#define IR_ANALYZER_MAX_SIGNALS 16

typedef struct {
    char     protocol[32];
    uint32_t address;
    uint32_t command;
    bool     repeat;
    bool     is_raw;
    uint32_t raw_count;
} IrAnalyzerSignal;

typedef struct {
    Gui*             gui;
    ViewPort*        view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    InfraredWorker*  ir_worker;

    IrAnalyzerSignal signals[IR_ANALYZER_MAX_SIGNALS];
    uint32_t         signal_count;
    bool             running;
} IrAnalyzerApp;
