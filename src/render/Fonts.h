#pragma once

struct ImFont;

namespace Glacier {

// Loads three Segoe-UI sizes (sm/md/lg) and merges Font Awesome into each.
class Fonts {
public:
    static Fonts& get();

    void load();

    ImFont* sm() const { return sm_; }
    ImFont* md() const { return md_; }
    ImFont* lg() const { return lg_; }
    ImFont* xl() const { return xl_; }

private:
    Fonts() = default;
    ImFont* sm_ = nullptr;
    ImFont* md_ = nullptr;
    ImFont* lg_ = nullptr;
    ImFont* xl_ = nullptr;
};

} // namespace Glacier
