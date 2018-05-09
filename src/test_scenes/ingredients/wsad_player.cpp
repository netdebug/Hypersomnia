#include "test_scenes/ingredients/ingredients.h"
#include "game/transcendental/entity_handle.h"
#include "game/transcendental/cosmos.h"

#include "game/assets/all_logical_assets.h"

#include "game/components/crosshair_component.h"
#include "game/components/sprite_component.h"
#include "game/components/movement_component.h"
#include "game/components/animation_component.h"
#include "game/components/fixtures_component.h"
#include "game/components/rigid_body_component.h"
#include "game/components/driver_component.h"
#include "game/components/force_joint_component.h"
#include "game/components/sentience_component.h"
#include "game/components/attitude_component.h"
#include "game/components/flags_component.h"
#include "game/components/shape_polygon_component.h"
#include "game/stateless_systems/particles_existence_system.h"

#include "game/enums/filters.h"
#include "game/enums/party_category.h"
#include "game/detail/inventory/perform_transfer.h"

namespace test_flavours {
	void populate_character_flavours(const loaded_image_caches_map& logicals, all_entity_flavours& flavours) {
		{
			auto& meta = get_test_flavour(flavours, test_controlled_characters::PLAYER);

			meta.get<invariants::text_details>().description = "Member of Atlantic nations.";

			{
				invariants::render render_def;
				render_def.layer = render_layer::SMALL_DYNAMIC_BODY;

				meta.set(render_def);
			}

			add_sprite(meta, logicals, test_scene_image_id::STANDARD_HEAD);
			add_shape_invariant_from_renderable(meta, logicals);

			{
				invariants::flags flags_def;
				flags_def.values.set(entity_flag::IS_PAST_CONTAGIOUS);
				meta.set(flags_def);
			}

			invariants::rigid_body body;
			invariants::fixtures fixtures_invariant;

			body.angled_damping = true;
			body.allow_sleep = false;

			body.damping.linear = 6.5f;
			body.damping.angular = 6.5f;

			fixtures_invariant.filter = filters::controlled_character();
			fixtures_invariant.density = 1.0;

			meta.set(body);
			meta.set(fixtures_invariant);

			{
				invariants::container container; 

				{
					inventory_slot slot_def;
					slot_def.physical_behaviour = slot_physical_behaviour::CONNECT_AS_FIXTURE_OF_BODY;
					slot_def.always_allow_exactly_one_item = true;
					slot_def.category_allowed = item_category::GENERAL;
					container.slots[slot_function::PRIMARY_HAND] = slot_def;
				}

				{
					inventory_slot slot_def;
					slot_def.physical_behaviour = slot_physical_behaviour::CONNECT_AS_FIXTURE_OF_BODY;
					slot_def.always_allow_exactly_one_item = true;
					slot_def.category_allowed = item_category::GENERAL;
					container.slots[slot_function::SECONDARY_HAND] = slot_def;
				}

				{
					inventory_slot slot_def;
					slot_def.category_allowed = item_category::SHOULDER_CONTAINER;
					slot_def.physical_behaviour = slot_physical_behaviour::CONNECT_AS_FIXTURE_OF_BODY;
					slot_def.always_allow_exactly_one_item = true;
					container.slots[slot_function::SHOULDER] = slot_def;
				}

				meta.set(container);
			}

			invariants::sentience sentience; 
			components::sentience sentience_inst;

			sentience.health_decrease_particles.id = to_particle_effect_id(test_scene_particle_effect_id::HEALTH_DAMAGE_SPARKLES);
			sentience.health_decrease_particles.modifier.colorize = red;
			sentience.health_decrease_particles.modifier.scale_lifetimes = 1.5f;

			sentience.health_decrease_sound.id = to_sound_id(test_scene_sound_id::IMPACT);
			sentience.death_sound.id = to_sound_id(test_scene_sound_id::DEATH);

			sentience.loss_of_consciousness_sound.id = to_sound_id(test_scene_sound_id::DEATH);
			sentience.consciousness_decrease_sound.id = to_sound_id(test_scene_sound_id::IMPACT);

			sentience_inst.get<health_meter_instance>().set_value(100);
			sentience_inst.get<health_meter_instance>().set_maximum_value(100);

			meta.set(sentience);
			meta.set(sentience_inst);

			invariants::movement movement;

			movement.input_acceleration_axes.set(1, 1);
			movement.acceleration_length = 10000;
			movement.braking_damping = 12.5f;
			movement.standard_linear_damping = 20.f;

			meta.set(movement);

			{
				invariants::crosshair crosshair; 
				crosshair.appearance.set(to_image_id(test_scene_image_id::TEST_CROSSHAIR), logicals);

				crosshair.recoil_damping.linear = { 5, 5 };
				crosshair.recoil_damping.angular = 5;

				meta.set(crosshair);
			}

			{
				components::crosshair crosshair;
				crosshair.base_offset.set(-20, 0);
				crosshair.sensitivity.set(3, 3);
				crosshair.base_offset_bound.set(1920, 1080);
				meta.set(crosshair);
			}

			{
				components::attitude attitude;

				attitude.parties = party_category::METROPOLIS_CITIZEN;
				attitude.hostile_parties = party_category::RESISTANCE_CITIZEN;

				meta.set(attitude);
			}

			{
				invariants::item_slot_transfers transfers; 
				meta.set(transfers);
			}
		}
	}
}

namespace prefabs {
	entity_handle create_sample_complete_character(
		const logic_step step, 
		const components::transform spawn_transform, 
		const std::string /* name */
	) {
		auto& world = step.get_cosmos();
		return create_test_scene_entity(world, test_controlled_characters::PLAYER, spawn_transform);
	}
}
