#pragma once
#include "../Module.h"

namespace Glacier::modules {

// Increases the gamma slider to maximum once enabled. Real implementation
// patches Options::gamma each tick (or hooks the gamma getter); here we
// store the setting and toggle a flag for the future Options hook.
class Fullbright final : public Module {
public:
    Fullbright();
    void onEnable()  override;
    void onDisable() override;
private:
    FloatSetting& gamma_;
};

} // namespace Glacier::modules
