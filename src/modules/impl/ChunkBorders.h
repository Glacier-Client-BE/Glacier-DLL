#pragma once
#include "../ModuleBase.h"
class ChunkBorders : public ModuleBase {
public:
    ChunkBorders();
    void onRender(ImDrawList* dl) override;
};
