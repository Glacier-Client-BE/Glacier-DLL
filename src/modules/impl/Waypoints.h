#pragma once
#include "../ModuleBase.h"
#include <vector>
#include <string>

struct Waypoint {
    std::string name;
    float x, y, z;
    int   dimension;
    float r, g, b;
};

class Waypoints : public ModuleBase {
public:
    Waypoints();
    void onRender(ImDrawList* dl) override;
    void onRenderImGui() override;
private:
    std::vector<Waypoint> m_waypoints;
    char m_newName[64]  = {};
    int  m_editIndex    = -1;
};
