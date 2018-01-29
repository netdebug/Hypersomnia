#include "game/common_state/entity_flavours.h"
#include "test_scenes/test_scene_flavours.h"
#include "test_scenes/ingredients/ingredients.h"

#include "augs/templates/enum_introspect.h"
#include "augs/templates/format_enum.h"

void populate_test_scene_flavours(const loaded_game_image_caches& logicals, entity_flavours& into) {
	into.flavours.resize(static_cast<std::size_t>(test_scene_flavour::COUNT));

	augs::for_each_enum([&](const test_scene_flavour e) {
		auto& new_type = into.flavours[static_cast<std::size_t>(e)];
		new_type.name = to_wstring(format_enum(e));
	});

	test_flavours::populate_grenade_types(logicals, into);
	test_flavours::populate_character_types(logicals, into);
	test_flavours::populate_gun_types(logicals, into);
	test_flavours::populate_other_types(logicals, into);
	test_flavours::populate_car_types(logicals, into);
	test_flavours::populate_crate_types(logicals, into);
	test_flavours::populate_melee_types(logicals, into);
	test_flavours::populate_backpack_types(logicals, into);

	/* Let all renderables have interpolation by default */

	for (auto& t : into.flavours) {
		if (t.find<invariants::render>()) {
			t.set(invariants::interpolation());
		}
	}
}