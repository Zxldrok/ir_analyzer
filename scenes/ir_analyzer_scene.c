#include "ir_analyzer_scene.h"

static const AppSceneOnEnterCallback on_enter_handlers[] = {
    ir_analyzer_scene_main_on_enter,
    ir_analyzer_scene_signal_list_on_enter,
    ir_analyzer_scene_signal_detail_on_enter,
};

static const AppSceneOnEventCallback on_event_handlers[] = {
    ir_analyzer_scene_main_on_event,
    ir_analyzer_scene_signal_list_on_event,
    ir_analyzer_scene_signal_detail_on_event,
};

static const AppSceneOnExitCallback on_exit_handlers[] = {
    ir_analyzer_scene_main_on_exit,
    ir_analyzer_scene_signal_list_on_exit,
    ir_analyzer_scene_signal_detail_on_exit,
};

const SceneManagerHandlers ir_analyzer_scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers  = on_exit_handlers,
    .scene_num         = IrAnalyzerSceneCount,
};
