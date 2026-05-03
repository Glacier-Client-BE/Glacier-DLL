#pragma once
#include "../Module.h"

namespace Glacier::modules {

class PlayerList final : public Module {
public:
    PlayerList();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  position_;
    BoolSetting&  showPing_;
    FloatSetting& width_;
};

} // namespace Glacier::modules
