#pragma once
#include "../Module.h"

namespace Glacier::modules {

class NoHurtCam final : public Module {
public:
    NoHurtCam();
private:
    FloatSetting& strength_;
};

} // namespace Glacier::modules
