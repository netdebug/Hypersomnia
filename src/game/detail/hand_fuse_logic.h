#pragma once
#include "game/cosmos/entity_handle.h"
#include "game/cosmos/entity_id.h"
#include "game/cosmos/step_declaration.h"
#include "game/detail/inventory/perform_transfer.h"
#include "game/detail/entity_handle_mixins/get_owning_transfer_capability.hpp"
#include "game/messages/start_sound_effect.h"
#include "game/detail/visible_entities.h"
#include "game/detail/physics/shape_overlapping.hpp"
#include "game/detail/bombsite_in_range.h"
#include "game/messages/battle_event_message.h"

template <class E>
struct fuse_logic_provider {
	const E& fused_entity;
	const logic_step step;
	components::hand_fuse& fuse;
	const invariants::hand_fuse& fuse_def;
	const transformr fused_transform;
	const entity_handle holder;
	cosmos& cosm;
	const augs::stepped_clock& clk;

	const entity_handle character_now_defusing;
	vec2 character_now_defusing_pos;

	template <class T>
	static bool is_standing_still(const T& who) {
		if (const auto movement = who.template find<components::movement>()) {
			return !movement->flags.any_moving_requested();
		}

		return false;
	}

	fuse_logic_provider(const E& fused_entity, const logic_step step) : 
		fused_entity(fused_entity), 
		step(step),
		fuse(fused_entity.template get<components::hand_fuse>()),
		fuse_def(fused_entity.template get<invariants::hand_fuse>()),
		fused_transform(fused_entity.get_logic_transform()),
		holder(fused_entity.get_owning_transfer_capability()),
		cosm(step.get_cosmos()),
		clk(cosm.get_clock()),
		character_now_defusing(cosm[fuse.character_now_defusing])
	{
		if (const auto t = character_now_defusing.find_logic_transform()) {
			character_now_defusing_pos = t->pos;
		}
	}

	bool arming_delay_complete() const {
		const auto when_started = fuse.when_started_arming;
		return when_started.was_set() && clk.is_ready(fuse_def.arming_duration_ms, when_started);
	}

	bool defusing_delay_complete() const {
		return fuse.amount_defused >= fuse_def.defusing_duration_ms;
	}

	void interrupt_arming() const {
		fuse.when_started_arming = {};
		stop_started_arming_sound();
	}

	void release_explosive_if_armed() const {
		if (fuse.armed()) {
			release_explosive();
		}

		interrupt_arming();
	}

	void release_explosive() const {
		// const auto& explosive = fused_entity.template get<invariants::explosive>();

		const auto total_impulse = [&]() {
			auto impulse = fuse_def.additional_release_impulse;

			if (const auto capability = holder.template find<invariants::item_slot_transfers>()) {
				const auto considered_impulse = 
					fuse.armed_as_secondary_action ? 
					capability->standard_drop_impulse :
					capability->standard_throw_impulse
				;

				impulse = impulse + considered_impulse;
			}

			return impulse;
		}();

		auto request = item_slot_transfer_request::drop(fused_entity, total_impulse);

		if (fuse_def.override_release_impulse) {
			request.params.apply_standard_impulse = false;
			request.params.additional_drop_impulse = fuse_def.additional_release_impulse;
		}

		/* 
			Note that the dropping transfer will effectively reinfer
			the colliders and the rigid body of the explosive,
			so no further refreshing is required.
		*/

		perform_transfer(request, step);

		fuse_def.release_sound.start(
			step,
			sound_effect_start_input::fire_and_forget(fused_transform).set_listener(holder)
		);
	}

	void refresh_fused_physics() const {
		fused_entity.infer_rigid_body();
		fused_entity.infer_colliders();
	}

	void defuse() const {
		fuse.when_armed = {};
	}

	void stop_started_arming_sound() const {
		const auto id = fused_entity.get_id();
		const auto& effect = fuse_def.started_arming_sound;

		messages::stop_sound_effect stop;
		stop.match_chased_subject = id;
		stop.match_effect_id = effect.id;
		step.post_message(stop);
	}

	void play_started_arming_sound() const {
		const auto id = fused_entity.get_id();
		const auto& effect = fuse_def.started_arming_sound;

		effect.start(
			step,
			sound_effect_start_input::orbit_local(id, {}).set_listener(holder)
		);

		stop_started_arming_sound();
	}

	void play_started_defusing_sound() const {
		messages::battle_event_message msg;
		msg.subject = character_now_defusing;
		msg.event = battle_event::IM_DEFUSING_THE_BOMB;

		step.post_message(msg);

		fuse_def.started_defusing_sound.start(
			step,
			sound_effect_start_input::fire_and_forget(fused_transform).set_listener(character_now_defusing)
		);
	}

	void play_defused_effects() const {
		for (std::size_t i = 0; i < fuse_def.defused_sound.size(); ++i) {
			fuse_def.defused_sound[i].start(
				step,
				sound_effect_start_input::fire_and_forget(fused_transform).set_listener(character_now_defusing)
			);
		}

		fuse_def.defused_particles.start(
			step,
			particle_effect_start_input::fire_and_forget(fused_transform)
		);
	}

	void start_arming() const {
		fuse.when_started_arming = clk.now;
	}

	void start_defusing() const {
		fuse.amount_defused = 0.f;
	}

	bool has_started_arming() const {
		return fuse.when_started_arming.was_set();
	}

	bool has_started_defusing() const {
		return fuse.amount_defused >= 0.f;
	}

	void arm_explosive() const {
		fuse.when_armed = clk.now;

		fused_entity.template get<components::sender>().set(holder);

		fuse_def.armed_sound.start(
			step,
			sound_effect_start_input::fire_and_forget(fused_transform).set_listener(holder)
		);

		if (fuse_def.always_release_when_armed) {
			release_explosive();

			if (const auto sentience = holder.template find<components::sentience>()) {
				sentience->hand_flags = {};
			}
		}
	}

	void interrupt_defusing() const {
		messages::battle_event_message msg;
		msg.subject = character_now_defusing;
		msg.event = battle_event::INTERRUPTED_DEFUSING;
		step.post_message(msg);

		if (character_now_defusing.alive()) {
			if (const auto sentience = character_now_defusing.find<components::sentience>()) {
				auto& u = sentience->use_button;

				if (u == use_button_state::DEFUSING) {
					u = use_button_state::QUERYING;
				}
			}
		}

		fuse.character_now_defusing = {}; 
		fuse.amount_defused = -1.f;
	}

	bool bombsite_in_range() const {
		return ::bombsite_in_range(fused_entity);
	}

	bool arming_conditions_fulfilled() const {
		if (holder.dead()) {
			return false;
		}

		if (fuse_def.must_stand_still_to_arm) {
			if (!is_standing_still(holder)) {
				return false;
			}
		}

		if (!bombsite_in_range()) {
			return false;
		}

		return true;
	}

	bool defusing_character_in_range() const {
		return 
			character_now_defusing.get_official_faction() != fused_entity.get_official_faction() 
			&& use_button_overlaps(character_now_defusing, fused_entity).has_value()
		;
	}

	bool defusing_conditions_fulfilled() const {
		return 
			fuse_def.defusing_enabled()
			&& character_now_defusing.alive() 
			&& character_now_defusing.get_official_faction() != fused_entity.get_official_faction()
			&& is_standing_still(character_now_defusing)
		;
	}

	bool arming_requested() const {
		return holder.alive() && fuse.arming_requested;
	}

	bool defusing_requested() const {
		return character_now_defusing.alive() && character_now_defusing.get<components::sentience>().use_button == use_button_state::DEFUSING; 
	}

	void advance_arming_and_defusing() const {
		if (fuse_def.has_delayed_arming()) {
			if (!fuse.armed()) {
				if (arming_requested()) {
					if (arming_delay_complete()) {
						arm_explosive();
						return;
					}

					if (arming_conditions_fulfilled()) {
						if (!has_started_arming()) {
							start_arming();
							play_started_arming_sound();
							return;
						}
					}
					else {
						interrupt_arming();
					}
				}
				else {
					interrupt_arming();
				}
			}
		}

		if (fuse_def.defusing_enabled()) {
			if (fuse.armed()) {
				if (defusing_requested()) {
					if (defusing_delay_complete()) {
						defuse();
						play_defused_effects();
						refresh_fused_physics();
						return;
					}

					if (defusing_conditions_fulfilled() && defusing_character_in_range()) {
						if (!has_started_defusing()) {
							start_defusing();
							play_started_defusing_sound();
							return;
						}
						
						const auto n = character_now_defusing.get_wielded_items().size();

						real32 defusing_speed_mult = 1.f;

						if (const auto maybe_defuser = character_now_defusing[slot_function::BELT].get_item_if_any()) {
							if (const auto tool = maybe_defuser.template find<invariants::tool>()) {
								defusing_speed_mult *= tool->defusing_speed_mult;
							}
						}

						if (n == 2) {
							defusing_speed_mult *= 0.5f;
						}
						else if (n == 0) {
							defusing_speed_mult *= 2.f;
						}

						fuse.amount_defused += defusing_speed_mult * clk.dt.in_milliseconds();
					}
					else {
						interrupt_defusing();
					}
				}
				else {
					interrupt_defusing();
				}
			}
		}
	}
};
