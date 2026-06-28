#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <infrared/infrared_worker.h>
#include <infrared/infrared.h>
#include <notification/notification_messages.h>
#include <furi.h>
#include <furi_hal.h>

#define IR_ANALYZER_MAX_SIGNALS 32

typedef enum {
    IrAnalyzerViewMain,
    IrAnalyzerViewSignalList,
    IrAnalyzerViewSignalDetail,
    IrAnalyzerViewCount,
} IrAnalyzerView;

typedef enum {
    IrAnalyzerSceneMain,
    IrAnalyzerSceneSignalList,
    IrAnalyzerSceneSignalDetail,
    IrAnalyzerSceneCount,
} IrAnalyzerScene;

typedef struct {
    char     protocol[32];
    uint32_t address;
    uint32_t command;
    bool     repeat;
    bool     is_raw;
    uint32_t raw_timings[256];
    uint32_t raw_count;
} IrAnalyzerSignal;

typedef struct {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    NotificationApp* notifications;
    InfraredWorker*  ir_worker;

    // Vues
    View*     view_main;
    Submenu*  submenu;
    Widget*   widget_detail;

    // Données
    IrAnalyzerSignal signals[IR_ANALYZER_MAX_SIGNALS];
    uint32_t         signal_count;
    uint32_t         selected_signal;
    bool             signal_received;
} IrAnalyzerApp;

IrAnalyzerApp* ir_analyzer_app_alloc(void);
void           ir_analyzer_app_free(IrAnalyzerApp* app);
int32_t        ir_analyzer_app(void* p);
