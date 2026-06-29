#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <string.h>
#include <stdio.h>

#define IR_MAX_SIGNALS    32
#define IR_MAX_TIMINGS    256
#define IR_SAVE_PATH      "/ext/infrared/ir_analyzer"
#define TURBO_MAX         5

typedef enum {
    ViewLive,
    ViewList,
    ViewDetail,
    ViewTransmit,
} AppView;

typedef enum {
    EventInput,
    EventSignal,
    EventTxComplete,
    EventNotifyGreen,
    EventNotifyYellow,
} EventType;

typedef struct {
    EventType  type;
    InputEvent input;
} AppEvent;

typedef enum {
    TxIdle,
    TxActive,
} TxState;

typedef struct {
    bool     is_raw;
    char     info[64];
    uint32_t timings[IR_MAX_TIMINGS];
    uint32_t timing_count;
    uint32_t seen_count;
    uint32_t frequency;
    float    duty_cycle;
    InfraredMessage message;
} IrSignal;

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* queue;
    NotificationApp*  notifications;
    InfraredWorker*   worker;
    IrSignal          signals[IR_MAX_SIGNALS];
    uint32_t          signal_count;
    bool              running;

    AppView           view;
    int32_t           list_index;

    uint32_t          turbo_repeat;
    uint32_t          tx_remaining;
    uint32_t          tx_completed;
    int32_t           tx_sig_idx;
    TxState           tx_state;

    int32_t           last_sig_idx;

    uint32_t          session_start;
} IrApp;
