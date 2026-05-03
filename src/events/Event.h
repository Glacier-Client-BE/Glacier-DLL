#pragma once
//
// Base event type. Derived events should inherit and add fields.
// Cancellable events expose `cancel()` / `isCancelled()`.
//

namespace Glacier {

class Event {
public:
    virtual ~Event() = default;
};

class CancellableEvent : public Event {
public:
    void cancel(bool v = true) { cancelled_ = v; }
    [[nodiscard]] bool isCancelled() const { return cancelled_; }
private:
    bool cancelled_ = false;
};

} // namespace Glacier
