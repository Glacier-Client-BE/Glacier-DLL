#pragma once
#include "../Module.h"

namespace Glacier::modules {

class ChunkBorders final : public Module {
public:
    ChunkBorders();
    void onRender(RenderHUDEvent& e);
private:
    BoolSetting&  showSubChunks_;
    ColorSetting& color_;
};

} // namespace Glacier::modules
