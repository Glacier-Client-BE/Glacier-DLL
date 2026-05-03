#pragma once
#include <string_view>

namespace Glacier {

enum class Category {
    HUD,
    Visual,
    Movement,
    World,
    Misc,
    Combat,
    _Count
};

constexpr std::string_view kCategoryName(Category c) {
    switch (c) {
        case Category::HUD:      return "HUD";
        case Category::Visual:   return "Visual";
        case Category::Movement: return "Movement";
        case Category::World:    return "World";
        case Category::Misc:     return "Misc";
        case Category::Combat:   return "Combat";
        default:                 return "?";
    }
}

constexpr int kCategoryCount = static_cast<int>(Category::_Count);

}
