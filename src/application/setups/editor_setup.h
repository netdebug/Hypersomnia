#pragma once
#include "application/debug_settings.h"

#include "game/transcendental/cosmos.h"
#include "game/transcendental/entity_handle.h"
#include "application/debug_character_selection.h"
#include "augs/misc/debug_entropy_player.h"
#include "game/assets/all_logical_assets.h"
#include "augs/misc/fixed_delta_timer.h"
#include "game/organization/all_component_includes.h"

class editor_setup {
public:
	cosmos subject_cosmos;
	cosmic_entropy total_collected_entropy;
	augs::fixed_delta_timer timer = { 5, augs::lag_spike_handling_type::DISCARD };
	entity_id viewed_entity_id;
	all_logical_assets logical_assets;

	editor_setup(const std::string& directory);

	void save(const std::string& directory);
	void load(const std::string& directory);

	auto get_audiovisual_speed() const {
		return 1.0;
	}

	auto get_interpolation_ratio() const {
		return timer.fraction_of_step_until_next_step(subject_cosmos.get_fixed_delta());
	}

	entity_handle get_viewed_character() {
		return subject_cosmos[viewed_entity_id];
	}

	const auto& get_viewing_cosmos() const {
		return subject_cosmos;
	}

	template <class F, class G>
	void advance(
		F&& advance_audiovisuals, 
		G&& step_post_solve
	) {
		auto steps = timer.extract_num_of_logic_steps(subject_cosmos.get_fixed_delta());

		if (!steps) {
			advance_audiovisuals();
		}

		while (steps--) {
			player.advance_player_and_biserialize(total_collected_entropy);

			hypersomnia.advance(
				{ total_collected_entropy, logical_assets },
				[](auto){},
				std::forward<G>(step_post_solve)
			);

			total_collected_entropy.clear();
			advance_audiovisuals();
		}
	}

	void control(
		const cosmic_entropy&
	);
};