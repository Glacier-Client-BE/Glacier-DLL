#include "Fullbright.h"
#include "../../core/Logger.h"

namespace Glacier::modules {

Fullbright::Fullbright()
    : Module("fullbright", "Fullbright", "Maximum gamma for total visibility", Category::Visual, 'B'),
      gamma_(addSetting<FloatSetting>("gamma", "Gamma", "Target gamma value", 5.0f, 1.f, 20.f, 0.5f))
{}

void Fullbright::onEnable()  { Logger::get().info("Mod", "Fullbright -> patching gamma to ", gamma_.get()); }
void Fullbright::onDisable() { Logger::get().info("Mod", "Fullbright -> restoring gamma");                 }

} // namespace Glacier::modules
