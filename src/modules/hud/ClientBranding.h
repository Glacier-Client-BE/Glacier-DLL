#pragma once
#include "../Module.h"

namespace Glacier::modules {

// Subtle brand chip pinned to the bottom-right (separate from the main
// Watermark so it can be enabled independently for screenshots).
class ClientBranding final : public Module {
public:
    ClientBranding();
    void onRender(RenderHUDEvent& e);
private:
    FloatSetting& opacity_;
};

} // namespace Glacier::modules
