#pragma once
#include "../Module.h"

namespace Glacier::modules {

class AutoGG final : public Module {
public:
    AutoGG();
    void onChat(ChatReceiveEvent& e);
private:
    TextSetting&  message_;
    TextSetting&  triggers_; // comma-separated
    IntSetting&   delayMs_;
};

} // namespace Glacier::modules
