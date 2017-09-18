#pragma once
#include "augs/math/transform.h"

template <class T>
struct basic_camera_cone {
	using vec2 = basic_vec2<T>;

	basic_transform<T> transform;
	vec2 visible_world_area;

	vec2 operator[](const vec2 pos) const {
		return pos - transform.pos + visible_world_area / 2;
	}

	vec2 get_screen_space_0_1(const vec2 pos) const {
		return operator[](pos) / visible_world_area;
	}

	vec2 get_screen_space_revert_y(const vec2 pos) const {
		auto ss = operator[](pos);
		ss.y = visible_world_area.y - ss.y;
		return ss;
	}

	ltrb get_transformed_visible_world_area_aabb() const {
		return augs::get_aabb_rotated(visible_world_area, transform.rotation) + transform.pos - visible_world_area / 2;
	}
};

using camera_cone = basic_camera_cone<float>;