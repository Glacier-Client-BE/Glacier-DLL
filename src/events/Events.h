#pragma once
//
// All event types live here. Adding an event = drop a struct + (optionally) a
// hook somewhere that calls EventBus::get().dispatch<E>(e).
//
#include "Event.h"
#include <string>
#include <cstdint>

struct ImDrawList;

namespace Glacier {

// ---- Per-frame -------------------------------------------------------------
struct TickEvent : public Event {
    float deltaSeconds = 0.0f;
};

// Dispatched once per frame, before any HUD rendering. Modules that draw
// using ImGui's foreground draw list subscribe here.
struct RenderHUDEvent : public Event {
    ImDrawList* drawList = nullptr;
    float       width    = 0.0f;
    float       height   = 0.0f;
};

// Dispatched once per frame inside the ImGui window pass. Modules that want
// regular ImGui windows (not just primitive shapes) subscribe here.
struct RenderImGuiEvent : public Event {};

// ---- Input -----------------------------------------------------------------
// Maps to WM_KEYDOWN / WM_KEYUP / WM_LBUTTONDOWN etc.
struct KeyEvent : public CancellableEvent {
    int  vkey       = 0;     // VK_* virtual-key code, including VK_LBUTTON / VK_RBUTTON
    bool down       = false; // down or up
    bool consumedByGui = false; // set true if the click-GUI ate this key
};

// Mouse-move delta. Useful for FreeLook / Zoom modules.
struct MouseMoveEvent : public Event {
    float dx = 0.0f;
    float dy = 0.0f;
};

// ---- Game (placeholders - dispatched by future hooks) ---------------------
struct ChatSendEvent : public CancellableEvent {
    std::string message;
};

struct ChatReceiveEvent : public CancellableEvent {
    std::string sender;
    std::string message;
};

struct AttackEntityEvent : public CancellableEvent {
    void* victim = nullptr; // sdk::Actor*
};

struct PacketReceiveEvent : public CancellableEvent {
    int   id   = 0;
    void* pkt  = nullptr;
};

struct PacketSendEvent : public CancellableEvent {
    int   id   = 0;
    void* pkt  = nullptr;
};

} // namespace Glacier
