#pragma once
#include "../ModuleBase.h"
#include <deque>
#include <string>

struct PacketEntry { std::string text; bool inbound; };

class PacketLogger : public ModuleBase {
public:
    PacketLogger();
    void onEnable() override;
    void onRenderImGui() override;
    void log(const std::string& pkt, bool inbound = true);
private:
    std::deque<PacketEntry> m_log;
};
