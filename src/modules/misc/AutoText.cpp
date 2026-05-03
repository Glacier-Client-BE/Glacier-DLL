#include "AutoText.h"
#include "../../core/Logger.h"
#include "../../events/EventBus.h"

namespace Glacier::modules {

AutoText::AutoText()
    : Module("auto_text", "AutoText", "Repeating chat message", Category::Misc),
      message_    (addSetting<TextSetting>("message",      "Message",  "What to send",                std::string("Powered by Glacier Client"))),
      intervalSec_(addSetting<IntSetting> ("interval_sec", "Interval", "Seconds between repeats",     30, 5, 600))
{
    EventBus::get().listen<TickEvent, &AutoText::onTick>(this);
}

void AutoText::onTick(TickEvent&) {
    if (!enabled_) return;
    auto now = std::chrono::steady_clock::now();
    if (now < next_) return;
    next_ = now + std::chrono::seconds(intervalSec_.get());
    Logger::get().info("AutoText", "Would send: ", message_.get());
}

} // namespace Glacier::modules
