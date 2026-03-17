#include "SafeWalk.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

SafeWalk::SafeWalk()
    : ModuleBase("Safe Walk","Prevents falling off edges",ICON_FA_SHOE_PRINTS,ModuleCategory::Movement)
{
    m_settings.defineBool("onlyEdge","Only Near Edges",true);
    m_settings.defineBool("notify",  "Notify on Toggle",false);
}

void SafeWalk::onEnable()  {
    auto* lp = getLocalPlayer();
    if (lp) {
        m_wasSneaking = lp->isSneaking();
        lp->setSneaking(true);
    }
}

void SafeWalk::onDisable() {
    auto* lp = getLocalPlayer();
    if (lp && !m_wasSneaking) lp->setSneaking(false);
}

void SafeWalk::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp || !lp->isOnGround()) return;

    if (m_settings.getBool("onlyEdge")) {
        // Check the block below-forward by probing player Y-1 in movement dir
        // If there is no block, activate sneak; otherwise release it to avoid
        // walking slower on safe ground.
        auto* lvl = getLevel();
        if (!lvl) { lp->setSneaking(true); return; }

        Vec3 pos = lp->getPosition();
        // Look one block ahead in each cardinal direction
        int bx = (int)floorf(pos.x);
        int by = (int)floorf(pos.y) - 1; // one block below feet
        int bz = (int)floorf(pos.z);

        bool edgeN = lvl->getBlockId(bx,   by, bz-1) == 0;
        bool edgeS = lvl->getBlockId(bx,   by, bz+1) == 0;
        bool edgeW = lvl->getBlockId(bx-1, by, bz  ) == 0;
        bool edgeE = lvl->getBlockId(bx+1, by, bz  ) == 0;

        lp->setSneaking(edgeN || edgeS || edgeW || edgeE);
    } else {
        lp->setSneaking(true);
    }
}
