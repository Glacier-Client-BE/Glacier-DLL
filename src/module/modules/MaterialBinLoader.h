#pragma once

#include "../Module.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Loads custom shader packs: while enabled, the SDK's readAssetFile hook
// answers *.material.bin requests from the `materials/` folder next to the DLL
// (drop RenderDragon shader .material.bin files there). The substitution
// itself lives in GameSDK so the module is a pure on/off gate, like Fullbright.
// Already-loaded materials apply after the game reloads them (resource-pack
// reload or restart) — same behaviour as BetterRenderDragon.
class MaterialBinLoader final : public Module {
public:
    MaterialBinLoader()
        : Module("Material Bin Loader",
                 "Load custom shader / material-bin packs from the materials folder",
                 Category::Visual) {}

    void onEnable() override  { sdk::GameSDK::get().setMaterialRedirect(true); }
    void onDisable() override { sdk::GameSDK::get().setMaterialRedirect(false); }
};

} // namespace glacier
