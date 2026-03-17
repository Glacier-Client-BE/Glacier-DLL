#pragma once
#include "../ModuleBase.h"
class HitColor : public ModuleBase {
public:
    HitColor();
    void onRender(ImDrawList* dl) override;
private:
    bool m_flash=false;
    float m_fadeTimer=0.f;
    std::chrono::high_resolution_clock::time_point m_lastHit;
    bool m_prevLMB=false;
};
