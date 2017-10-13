#pragma once
#include "game/components/sprite_component.h"
#include "augs/misc/constant_size_vector.h"

namespace components {
	struct wandering_pixels {
		// GEN INTROSPECTOR struct components::wandering_pixels
		xywh reach = xywh(0.f, 0.f, 0.f, 0.f);
		augs::constant_size_vector<sprite, 10> frames;
		float frame_duration_ms = 300.f;
		unsigned particles_count = 0u;
		// END GEN INTROSPECTOR

		wandering_pixels() {
			frames.emplace_back();
		}

		const sprite& get_face_after(const float passed_lifetime_ms) const {
			const auto frame_count = frames.size();
			const auto frame_num = static_cast<unsigned>(passed_lifetime_ms / frame_duration_ms) % frame_count;

			return frames[frame_num];
		}
	};
}