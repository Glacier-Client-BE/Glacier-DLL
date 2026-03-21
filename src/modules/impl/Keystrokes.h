#pragma once
#include "../ModuleBase.h"
#include <deque>
#include <chrono>
#include <Windows.h>

class Keystrokes : public ModuleBase {
public:
    Keystrokes();
    void onRenderImGui() override;
private:
    // signature matches the .cpp exactly — no default parameter
    void drawKey(ImDrawList* dl, const char* lbl,
                 float x, float y, float w, float h, bool pressed) const;

    using Clock = std::chrono::high_resolution_clock;
    using TP    = Clock::time_point;
    std::deque<TP> m_cpsL, m_cpsR;
    bool m_pL = false, m_pR = false;
};
