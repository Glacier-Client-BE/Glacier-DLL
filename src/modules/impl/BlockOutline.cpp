#include "BlockOutline.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

BlockOutline::BlockOutline()
    : ModuleBase("Block Outline", "Customizes the selected block outline and overlay color",
                 ICON_FA_BORDER_ALL, ModuleCategory::Visual)
{
    m_settings.defineBool ("showOutline",  "Show Outline",      true);
    m_settings.defineFloat("outlineR",     "Outline R",         114.f, 0.f, 255.f);
    m_settings.defineFloat("outlineG",     "Outline G",         137.f, 0.f, 255.f);
    m_settings.defineFloat("outlineB",     "Outline B",         218.f, 0.f, 255.f);
    m_settings.defineFloat("outlineA",     "Outline Alpha",     1.0f,  0.f, 1.f);
    m_settings.defineBool ("full3D",       "3D Outline",        false);
    m_settings.defineBool ("showOverlay",  "Show Overlay",      false);
    m_settings.defineFloat("overlayR",     "Overlay R",         255.f, 0.f, 255.f);
    m_settings.defineFloat("overlayG",     "Overlay G",         255.f, 0.f, 255.f);
    m_settings.defineFloat("overlayB",     "Overlay B",         255.f, 0.f, 255.f);
    m_settings.defineFloat("overlayA",     "Overlay Alpha",     0.3f,  0.f, 1.f);
    m_settings.defineBool ("changeWidth",  "Custom Width",      false);
    m_settings.defineFloat("lineWidth",    "Line Width",        0.06f, 0.01f, 0.5f);
}

void BlockOutline::onEnable()  {}
void BlockOutline::onDisable() {}
