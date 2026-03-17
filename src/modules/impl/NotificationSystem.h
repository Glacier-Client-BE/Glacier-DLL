#pragma once
#include "../ModuleBase.h"
#include <string>
#include <deque>
#include <chrono>
class NotificationSystem : public ModuleBase {
public:
    NotificationSystem();
    void onRenderImGui() override;
    static void push(const std::string& title, const std::string& msg, ImU32 accent = IM_COL32(114,137,218,255));
    struct Notif { std::string title, msg; ImU32 accent; std::chrono::high_resolution_clock::time_point born; };
private:
    inline static std::deque<Notif> s_queue;
};
