#pragma once
#include "../ir_analyzer.h"
#include <gui/scene_manager.h>

// Déclarations des handlers de scènes
extern const SceneManagerHandlers ir_analyzer_scene_handlers;

// ── Scène Main ────────────────────────────────────────────────────────────────
void ir_analyzer_scene_main_on_enter(void* ctx);
bool ir_analyzer_scene_main_on_event(void* ctx, SceneManagerEvent event);
void ir_analyzer_scene_main_on_exit(void* ctx);

// ── Scène Signal List ─────────────────────────────────────────────────────────
void ir_analyzer_scene_signal_list_on_enter(void* ctx);
bool ir_analyzer_scene_signal_list_on_event(void* ctx, SceneManagerEvent event);
void ir_analyzer_scene_signal_list_on_exit(void* ctx);

// ── Scène Signal Detail ───────────────────────────────────────────────────────
void ir_analyzer_scene_signal_detail_on_enter(void* ctx);
bool ir_analyzer_scene_signal_detail_on_event(void* ctx, SceneManagerEvent event);
void ir_analyzer_scene_signal_detail_on_exit(void* ctx);
