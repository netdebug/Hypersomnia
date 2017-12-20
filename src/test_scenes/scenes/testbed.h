#pragma once
#include "game/transcendental/cosmic_entropy.h"
#include "game/transcendental/step_declaration.h"
#include "game/transcendental/cosmos.h"
#include "game/transcendental/standard_solver.h"

class cosmos;
struct all_logical_assets;

namespace test_scenes {
	class testbed {
		void populate(const logic_step) const;

	public:
		void populate(cosmos_common_state&) const;
		void populate_with_entities(const logic_step_input input) const {
			standard_solver(
				input,
				[&](const logic_step step) { populate(step); }, 
				[](auto) {},
				[](auto) {}
			);
		}
	};
}