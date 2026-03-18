#pragma once
#include "../ModuleBase.h"

class ChunkMap : public ModuleBase {
public:
    ChunkMap();
    void onRender(ImDrawList* dl) override;
};
