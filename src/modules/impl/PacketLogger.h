#pragma once
#include "../ModuleBase.h"
#include <deque>
#include <string>
class PacketLogger : public ModuleBase {
public:
    PacketLogger();
    void onRenderImGui() override;
    static void log(const std::string& pkt);
private:
    inline static std::deque<std::string> s_log;
};
