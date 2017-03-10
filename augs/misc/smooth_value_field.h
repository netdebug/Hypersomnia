#pragma once

#include "augs/math/vec2.h"
#include "augs/misc/timer.h"

namespace augs {
	class smooth_value_field {
	public:
		vec2i discrete_value;
		vec2 value;

		vec2 target_value;

		double averages_per_sec = 20.0;
		double smoothing_average_factor = 0.5;

		void tick(double delta_seconds);
	};
}