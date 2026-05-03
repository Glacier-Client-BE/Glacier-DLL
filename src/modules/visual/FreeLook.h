#pragma once
#include "../Module.h"

namespace Glacier::modules {

class FreeLook final : public Module {
public:
    FreeLook();
    void onKey(KeyEvent& k);
private:
    KeybindSetting& holdKey_;
    BoolSetting&    perspective_;
    bool active_ = false;
};

} // namespace Glacier::modules
