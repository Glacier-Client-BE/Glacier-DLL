#pragma once

#include <concepts>
#include <cstdint>

#include <entt/entity/registry.hpp>

// Bedrock entity components, reached through the game's own entt registry.
//
// ── Why this is the most fragile file in Glacier ──
//
// Minecraft moved entity data (position, equipment, rotation, …) out of fixed
// Actor fields and into an ECS. There is no longer an `Actor + 0xNNN` to read
// for armor; the data hangs off an entt registry keyed by a compile-time type
// hash. Reaching it means our entt must agree, bit for bit, with the entt the
// game was compiled against — the sparse-set paging, the entity id bit layout,
// and the storage type all have to line up.
//
// That is why third_party/entt is pinned to the exact commit Latite uses
// (fe8d7d78) rather than to a release tag or a vcpkg version. A different entt
// will still compile and will still "work" right up until it reads the wrong
// memory. If armor data ever comes back as plausible-but-wrong rather than
// absent, suspect this pin first.
//
// Everything below mirrors the traits Bedrock's own build uses. They are not
// arbitrary: entity_mask/version_mask and page_size must match, or the sparse
// set indexes into the wrong page and returns a component belonging to some
// other entity.

class EntityId;

struct EntityIdTraits {
    using value_type = EntityId;
    using entity_type = std::uint32_t;
    using version_type = std::uint16_t;

    static constexpr entity_type entity_mask = 0x3FFFF;
    static constexpr entity_type version_mask = 0x3FFF;
};

// Empty base tag. Bedrock's components derive from this; entt's traits below
// key off that, so it must stay empty (a vtable here would shift every field).
struct IEntityComponent {};

template <>
struct entt::entt_traits<EntityId> : entt::basic_entt_traits<EntityIdTraits> {
    static constexpr std::size_t page_size = 2048;
};

template <std::derived_from<IEntityComponent> Type>
struct entt::component_traits<Type, EntityId> {
    using element_type = Type;
    using entity_type = EntityId;
    static constexpr bool in_place_delete = true;
    static constexpr std::size_t page_size = 128 * !std::is_empty_v<Type>;
};

template <typename Type>
struct entt::storage_type<Type, EntityId> {
    using type = basic_storage<Type, EntityId>;
};

// Components are looked up by the hash Mojang assigned them, not by a hash of
// our type name — so every component below must carry the game's own constant.
template <std::derived_from<IEntityComponent> Type>
struct entt::type_hash<Type> {
    [[nodiscard]] static consteval id_type value() noexcept {
        constexpr auto hash = Type::type_hash;
        return hash;
    }
    [[nodiscard]] consteval operator id_type() const noexcept { return value(); }
};

class EntityId : public entt::entt_traits<EntityId> {
public:
    entity_type mRawId{};

    template <std::integral T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, bool>)
    constexpr EntityId(T rawId) : mRawId(static_cast<entity_type>(rawId)) {}

    [[nodiscard]] constexpr operator entity_type() const { return mRawId; }
    constexpr bool operator==(const EntityId& other) const { return mRawId == other.mRawId; }
};

namespace glacier::sdk {

// Actor's embedded context: which registry it belongs to and which entity it
// is. Laid out to match the game — two references then the id.
struct EntityContext {
    void*    registry;        // EntityRegistry& (unused by Glacier)
    entt::basic_registry<EntityId>* enttRegistry;
    EntityId entity{ 0u };
};

// Equipment. `armorContainer` is a Container whose getItem(int) is virtual at
// the index recorded in the signature table.
struct ActorEquipmentComponent : IEntityComponent {
    static constexpr std::uint32_t type_hash = 0xB06141A9;

    void* handContainer;
    void* armorContainer;
};
static_assert(sizeof(ActorEquipmentComponent) == 0x10,
              "ActorEquipmentComponent must stay two pointers — the game's layout");

} // namespace glacier::sdk
