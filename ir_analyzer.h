#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

#define IR_MAX_SIGNALS   16
#define IR_MAX_TIMINGS   128
#define IR_SAVE_PATH     "/ext/infrared/ir_analyzer"

typedef enum {
    ViewLive,
    ViewList,
    ViewDetail,
} AppView;

typedef struct {
    bool     is_raw;
    char     info[48];
    uint32_t timings[IR_MAX_TIMINGS];
    uint32_t timing_count;
    uint32_t seen_count;
} IrSignal;

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* queue;
    NotificationApp*  notifications;

    IrSignal          signals[IR_MAX_SIGNALS];
    uint32_t          signal_count;
    bool              running;

    AppView           view;
    int32_t           list_index;
} IrApp;
