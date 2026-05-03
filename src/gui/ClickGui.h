#pragma once
//
// Two-pane settings menu opened with INSERT (default).  Left rail = category
// list, right pane = modules in the selected category.  Each module is a
// glass-style card with toggle + expand-for-settings.
//
#include "../events/Events.h"
#include "../modules/Category.h"

namespace Glacier {

class ClickGui {
public:
    static ClickGui& get();

    void init(); // hooks the EventBus
    bool open() const { return open_; }
    void setOpen(bool v) { open_ = v; }

private:
    ClickGui() = default;

    void onKey(KeyEvent& e);
    void onRender(RenderImGuiEvent& e);

    bool        open_           = false;
    int         openKey_        = 0x2D;   // VK_INSERT
    Category    activeCategory_ = Category::HUD;
    std::string filter_;
};

} // namespace Glacier
