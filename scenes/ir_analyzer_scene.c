#include "ir_analyzer_scene.h"

// Table des handlers — ordre = IrAnalyzerScene enum
const SceneManagerHandlers ir_analyzer_scene_handlers = {
    .on_enter_handlers = {
        ir_analyzer_scene_main_on_enter,
        ir_analyzer_scene_signal_list_on_enter,
        ir_analyzer_scene_signal_detail_on_enter,
    },
    .on_event_handlers = {
        ir_analyzer_scene_main_on_event,
        ir_analyzer_scene_signal_list_on_event,
        ir_analyzer_scene_signal_detail_on_event,
    },
    .on_exit_handlers = {
        ir_analyzer_scene_main_on_exit,
        ir_analyzer_scene_signal_list_on_exit,
        ir_analyzer_scene_signal_detail_on_exit,
    },
    .scene_num = IrAnalyzerSceneCount,
};
