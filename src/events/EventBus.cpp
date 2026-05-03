#include "EventBus.h"
#include <algorithm>

namespace Glacier {

EventBus& EventBus::get() {
    static EventBus B;
    return B;
}

void EventBus::unlistenAll(void* self) {
    std::unique_lock lk(mu_);
    for (auto& [_, vec] : map_) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [self](const Slot& s) { return s.recv == self; }),
            vec.end());
    }
}

} // namespace Glacier
