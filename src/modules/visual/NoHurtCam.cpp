#include "NoHurtCam.h"

namespace Glacier::modules {

NoHurtCam::NoHurtCam()
    : Module("no_hurt_cam", "No Hurt Cam", "Suppress the damage tilt animation", Category::Visual),
      strength_(addSetting<FloatSetting>("strength", "Strength", "0 = full suppression, 1 = vanilla", 0.0f, 0.0f, 1.0f, 0.05f))
{}

} // namespace Glacier::modules
