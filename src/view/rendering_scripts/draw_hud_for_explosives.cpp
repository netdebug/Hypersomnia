#include "rendering_scripts.h"
#include "augs/drawing/drawing.h"
#include "game/cosmos/cosmos.h"
#include "game/cosmos/for_each_entity.h"
#include "game/components/sprite_component.h"
#include "game/components/hand_fuse_component.h"
#include "game/components/fixtures_component.h"

#include "game/components/interpolation_component.h"
#include "game/components/fixtures_component.h"
#include "view/audiovisual_state/systems/interpolation_system.h"
#include "game/detail/hand_fuse_math.h"
#include "game/detail/bombsite_in_range.h"
#include "view/game_drawing_settings.h"
#include "game/detail/gun/shell_offset.h"
#include "game/detail/gun/gun_cooldowns.h"

void draw_hud_for_explosives(const draw_hud_for_explosives_input in) {
	const auto dt = in.cosm.get_fixed_delta();

	const auto& cosm = in.cosm;
	const auto tex = in.circular_bar_tex;
	const auto t = in.only_type;
	const auto clk = cosm.get_clock();
	const auto global_time_seconds = in.global_time_seconds;

	auto draw_circle_at = [&](const auto& tr, const auto highlight_amount, const rgba first_col, const rgba second_col) {
		if (highlight_amount >= 0.f && highlight_amount <= 1.f) {
			const auto highlight_color = augs::interp(first_col, second_col, (1 - highlight_amount)* (1 - highlight_amount));

			in.output.aabb_centered(tex, vec2(tr.pos).discard_fract(), highlight_color);

			augs::special s;

			const auto full_rot = 360;
			const auto empty_angular_amount = full_rot * (1 - highlight_amount);

			s.v1.set(0.f, 0.f);
			s.v2.set(-90 + empty_angular_amount, -90) /= 180;

			in.specials.push_back(s);
			in.specials.push_back(s);
			in.specials.push_back(s);

			in.specials.push_back(s);
			in.specials.push_back(s);
			in.specials.push_back(s);
		}
	};

	auto draw_circle = [&](const auto& it, const auto highlight_amount, const rgba first_col, const rgba second_col, const vec2 offset = vec2::zero) {
		if (auto tr = it.find_viewing_transform(in.interpolation)) {
			tr->pos += offset;
			draw_circle_at(*tr, highlight_amount, first_col, second_col);
		}
	};

	const auto& watched_character = cosm[in.viewed_character_id];
	const auto watched_character_faction = watched_character ? watched_character.get_official_faction() : faction_type::SPECTATOR;

	auto is_authorized_faction = [&](const auto f) {
		return watched_character_faction != faction_type::SPECTATOR && f == watched_character_faction;
	};

	cosm.for_each_having<components::hand_fuse>(
		[&](const auto it) {
			const auto& fuse = it.template get<components::hand_fuse>();
			const auto& fuse_def = it.template get<invariants::hand_fuse>();

			auto do_draw_circle = [&](auto&&... args) {
				draw_circle(it, args...);
			};

			if (t == circular_bar_type::MEDIUM && fuse_def.has_delayed_arming()) {
				if (fuse.arming_requested) {
					if (const auto slot = it.get_current_slot()) {
						if (is_authorized_faction(slot.get_container().get_official_faction())) {
							auto first_col = white;
							auto second_col = red_violet;

							if (!::bombsite_in_range(it)) {
								first_col = red;
								second_col = red;
							}

							const auto when_started_arming = 
								fuse.when_started_arming.was_set() ? 
								fuse.when_started_arming.in_seconds(dt) :
								global_time_seconds
							;

							const auto highlight_amount = static_cast<float>(
								(global_time_seconds - when_started_arming)
								/ (fuse_def.arming_duration_ms / 1000.f) 
							);

							do_draw_circle(highlight_amount, first_col, second_col);
						}
					}
				}
			}

			if (fuse.armed()) {
				const auto highlight_amount = static_cast<float>(1 - (
					(global_time_seconds - fuse.when_armed.in_seconds(dt))
					/ (fuse_def.fuse_delay_ms / 1000.f) 
				));

				if (t == circular_bar_type::MEDIUM && fuse_def.has_delayed_arming()) {
					do_draw_circle(highlight_amount, white, red_violet);
				}
				else if (t == circular_bar_type::SMALL && !fuse_def.has_delayed_arming()) {
					do_draw_circle(highlight_amount, white, red_violet);
				}

				if (t == circular_bar_type::OVER_MEDIUM && fuse_def.defusing_enabled()) {
					if (const auto amount_defused = fuse.amount_defused; amount_defused >= 0.f) {
						if (const auto defusing_character = cosm[fuse.character_now_defusing]) {
							if (is_authorized_faction(defusing_character.get_official_faction())) {
								const auto highlight_amount = static_cast<float>(
									amount_defused / fuse_def.defusing_duration_ms
								);

								do_draw_circle(highlight_amount, white, red_violet);
							}
						}
					}
				}
			}
		}
	);

	if (t == circular_bar_type::SMALL) {
		const bool enemy_hud = in.settings.draw_enemy_hud;

		cosm.for_each_having<components::gun>(
			[&](const auto& it) {
				if (const auto tr = it.find_viewing_transform(in.interpolation)) {
					const auto& gun = it.template get<components::gun>();

					auto draw_progress = [&](const auto& amount) {
						auto shell_spawn_offset = ::calc_shell_offset(it);
						shell_spawn_offset.pos.rotate(tr->rotation);

						draw_circle(it, amount, white, red_violet, shell_spawn_offset.pos);
					};

					if (const auto chambering_duration = ::calc_current_chambering_duration(it); augs::is_positive_epsilon(chambering_duration)) {
						const auto& progress = gun.chambering_progress_ms;

						if (progress > 0.f) {
							if (!enemy_hud) { 
								if (const auto c = it.get_owning_transfer_capability()) {
									if (!is_authorized_faction(c.get_official_faction())) {
										return;
									}
								}
							}

							draw_progress(progress / chambering_duration);
						}
					}
					else {
						const auto& gun_def = it.template get<invariants::gun>();
						const auto cooldown = gun_def.shot_cooldown_ms;

						if (cooldown > 1000.f) {
							if (const auto slot = it.get_current_slot(); slot && slot.is_hand_slot()) {
								const auto r = clk.get_ratio_of_remaining_time(cooldown, gun.when_last_fired);
								const auto transfer_r = clk.get_ratio_of_remaining_time(
									gun_def.get_transfer_shot_cooldown(), 
									it.when_last_transferred()
								);

								const auto later_r = std::max(r, transfer_r);

								if (augs::is_positive_epsilon(later_r)) {
									draw_progress(1.f - later_r);
								}
							}
						}
					}
				}
			}
		);

		const auto& global = cosm.get_global_solvable();

		for (const auto& m : global.pending_item_mounts) {
			const auto& item = cosm[m.first];

			if (item.dead()) {
				continue;
			}

			const auto& request = m.second;

			const auto& progress = request.progress_ms;

			if (progress > 0.f) {
				const auto highlight_amount = 1.f - (progress / request.get_mounting_duration_ms(item));

				if (!enemy_hud) { 
					if (const auto c = item.get_owning_transfer_capability()) {
						if (!is_authorized_faction(c.get_official_faction())) {
							return;
						}
					}
				}

				if (!request.is_unmounting(item)) {
					if (const auto slot = cosm[request.target]) {
						const auto tr = slot.get_container().find_viewing_transform(in.interpolation);
						draw_circle_at(*tr, highlight_amount, white, red_violet);
					}
				}
				else {
					draw_circle(item, highlight_amount, white, red_violet);
				}
			}
		}
	}
}

void draw_beep_lights::operator()() {
	const auto clk = cosm.get_clock();

	cosm.for_each_having<components::hand_fuse>(
		[&](const auto it) {
			const auto& fuse = it.template get<components::hand_fuse>();

			if (fuse.armed()) {
				const auto& fuse_def = it.template get<invariants::hand_fuse>();
				auto beep_col = fuse_def.beep_color;

				if (beep_col.a) {
					const auto beep = beep_math { fuse, fuse_def, clk };

					if (const auto mult = beep.get_beep_light_mult(); mult > AUGS_EPSILON<real32>) {
						beep_col.mult_alpha(mult);
						if (const auto tr = it.find_viewing_transform(interpolation)) {
							output.aabb_centered(
								cast_highlight_tex,
								tr->pos,
								beep_col
							);
						}
					}
				}
			}
		}
	);
}
