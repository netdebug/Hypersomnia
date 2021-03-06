#pragma once
#include "augs/math/transform.h"
#include "game/cosmos/component_synchronizer.h"
#include "game/debug_drawing_settings.h"
#include "game/components/rigid_body_component.h"

class physics_world_cache;
class physics_system;

template <class E, class B>
void infer_damping(const E& handle, B& b);

template <class E>
class component_synchronizer<E, components::rigid_body> 
	: public synchronizer_base<E, components::rigid_body> 
{
	friend ::physics_world_cache;
	friend ::physics_system;

	auto find_cache() const {
		return handle.get_cosmos().get_solvable_inferred({}).physics.find_rigid_body_cache(handle);
	}

	const decltype(std::declval<E&>().get_cosmos().get_solvable_inferred({}).physics.find_rigid_body_cache(std::declval<E&>())->body.get()) body_ptr;

	auto find_body_impl() const {	
		using T = decltype(find_cache()->body.get());

		if (auto cache = find_cache()) {
			return cache->body.get();
		}

		return T(nullptr);
	}

	auto find_body() const {	
		return body_ptr;
	}

	auto body() const {
		auto maybe_body = find_body();
		ensure(maybe_body != nullptr);
		return *maybe_body;
	}

	template <class T>
	auto to_pixels(const T meters) const {
		return handle.get_cosmos().get_si().get_pixels(meters);
	}

	template <class T>
	auto to_meters(const T pixels) const {
		return handle.get_cosmos().get_si().get_meters(pixels);
	}

	using base = synchronizer_base<E, components::rigid_body>;
	using base::handle;

public:
	using base::base;
	using base::get_raw_component;

	component_synchronizer(
		const typename base::component_pointer c, 
		const E& h
	) : 
		base(c, h),
		body_ptr(find_body_impl())
	{}

	void infer_caches() const;

	void infer_damping() const {
		if (const auto body = find_body()) {
			::infer_damping(handle, *body);
		}
	}

	void set_velocity(const vec2) const;
	void set_angular_velocity(const float) const;

	void set_transform(const transformr&) const;
	
	template <class id_type>
	void set_transform(const id_type id) const {
		set_transform(handle.get_cosmos()[id].get_logic_transform());
	}

	void apply_force(const vec2) const;
	void apply_force(const vec2, const vec2 center_offset, const bool wake = true) const;
	void apply_impulse(const vec2) const;
	void apply_impulse(const vec2, const vec2 center_offset, const bool wake = true) const;

	void apply_angular_impulse(const float) const;

	void apply(impulse_input) const;

	template <class body_type>
	void update_after_step(const body_type& b) const {
		auto& body = get_raw_component({});

		body.physics_transforms.m_xf = b.m_xf;
		body.physics_transforms.m_sweep = b.m_sweep;

		body.velocity = vec2(b.GetLinearVelocity());
		body.angular_velocity = b.GetAngularVelocity();
	}

	bool is_constructed() const;

	auto& get_special() const {
		return get_raw_component({}).special;
	}

	vec2 get_velocity() const;
	float get_mass() const;
	float get_degrees() const;
	float get_radians() const;
	float get_degree_velocity() const;
	float get_radian_velocity() const;
	float get_inertia() const;
	vec2 get_position() const;
	transformr get_transform() const;
	vec2 get_mass_position() const;
	vec2 get_world_center() const;
	bool test_point(const vec2) const;

	std::optional<ltrb> find_aabb() const;
};

template <class E>
float component_synchronizer<E, components::rigid_body>::get_mass() const {
	return body().GetMass();
}

template <class E>
float component_synchronizer<E, components::rigid_body>::get_degrees() const {
	return get_radians() * RAD_TO_DEG<float>;
}

template <class E>
float component_synchronizer<E, components::rigid_body>::get_radians() const {
	return body().GetAngle();
}

template <class E>
float component_synchronizer<E, components::rigid_body>::get_degree_velocity() const {
	return get_radian_velocity() * RAD_TO_DEG<float>;
}

template <class E>
float component_synchronizer<E, components::rigid_body>::get_radian_velocity() const {
	return body().GetAngularVelocity();
}

template <class E>
float component_synchronizer<E, components::rigid_body>::get_inertia() const {
	return body().GetInertia();
}

template <class E>
vec2 component_synchronizer<E, components::rigid_body>::get_position() const {
	return to_pixels(body().GetPosition());
}

template <class E>
transformr component_synchronizer<E, components::rigid_body>::get_transform() const {
	return { get_position(), get_degrees() };
}

template <class E>
vec2 component_synchronizer<E, components::rigid_body>::get_mass_position() const {
	return to_pixels(body().GetWorldCenter());
}

template <class E>
vec2 component_synchronizer<E, components::rigid_body>::get_velocity() const {
	return to_pixels(body().GetLinearVelocity());
}

template <class E>
vec2 component_synchronizer<E, components::rigid_body>::get_world_center() const {
	return to_pixels(body().GetWorldCenter());
}

template <class E>
bool component_synchronizer<E, components::rigid_body>::test_point(const vec2 v) const {
	return body().TestPoint(b2Vec2(to_meters(v)));
}

template <class E>
bool component_synchronizer<E, components::rigid_body>::is_constructed() const {
	return find_body() != nullptr;
}

template <class E>
void component_synchronizer<E, components::rigid_body>::infer_caches() const {
	handle.get_cosmos().get_solvable_inferred({}).physics.infer_rigid_body(handle);
}

template <class E>
void component_synchronizer<E, components::rigid_body>::set_velocity(const vec2 pixels) const {
	auto& v = get_raw_component({}).velocity;
	v = to_meters(pixels);

	if (const auto body = find_body()) {
		body->SetLinearVelocity(b2Vec2(v));
	}
}

template <class E>
void component_synchronizer<E, components::rigid_body>::set_angular_velocity(const float degrees) const {
	auto& v = get_raw_component({}).angular_velocity;
	v = DEG_TO_RAD<float> * degrees;

	if (const auto body = find_body()) {
		body->SetAngularVelocity(v);
	}
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply_force(const vec2 pixels) const {
	apply_force(pixels, vec2(0, 0), true);
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply_force(
	const vec2 pixels, 
	const vec2 center_offset, 
	const bool wake
) const {
	apply_impulse(handle.get_cosmos().get_fixed_delta().in_seconds() * pixels, center_offset, wake);
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply(const impulse_input impulse) const {
	apply_impulse(impulse.linear, vec2(0, 0), true);
	apply_angular_impulse(impulse.angular);
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply_impulse(const vec2 pixels) const {
	apply_impulse(pixels, vec2(0, 0), true);
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply_impulse(
	const vec2 pixels, 
	const vec2 center_offset, 
	const bool wake
) const {
	if (pixels.is_epsilon(2.f)) {
		return;
	}

	if (const auto body = find_body()) {
		auto& data = get_raw_component({});

		const auto force = to_meters(pixels);
		const auto location = vec2(body->GetWorldCenter()) + to_meters(center_offset);

		body->ApplyLinearImpulse(
			b2Vec2(force), 
			b2Vec2(location), 
			wake
		);

		data.angular_velocity = body->GetAngularVelocity();
		data.velocity = body->GetLinearVelocity();

		if (DEBUG_DRAWING.draw_forces && pixels.is_nonzero()) {
			/* 
				Warning: bodies like player's crosshair recoil might have their forces drawn 
				in the vicinity of (0, 0) coordinates instead of near wherever the player is.
			*/

			auto& lines = DEBUG_LOGIC_STEP_LINES;
			lines.emplace_back(green, to_pixels(location) + pixels, to_pixels(location));
		}
	}
}

template <class E>
void component_synchronizer<E, components::rigid_body>::apply_angular_impulse(const float imp) const {
	if (auto body = find_body()) {
		auto& data = get_raw_component({});

		body->ApplyAngularImpulse(imp, true);
		data.angular_velocity = body->GetAngularVelocity();
	}
}

template <class E>
void component_synchronizer<E, components::rigid_body>::set_transform(const transformr& transform) const {
	auto& data = get_raw_component({});

	data.physics_transforms.set(
		handle.get_cosmos().get_common_significant().si,
		transform
	);

	if (const auto body = find_body()) {
		if (!(body->m_xf == data.physics_transforms.m_xf)) {
			body->m_xf = data.physics_transforms.m_xf;
			body->m_sweep = data.physics_transforms.m_sweep;

			auto* broadPhase = &body->m_world->m_contactManager.m_broadPhase;

			for (auto* f = body->m_fixtureList; f; f = f->m_next)
			{
				f->Synchronize(broadPhase, body->m_xf, body->m_xf);
			}
		}
	}	
}

class b2Body;
struct b2AABB;

std::optional<b2AABB> find_aabb(const b2Body& body);

template <class E>
std::optional<ltrb> component_synchronizer<E, components::rigid_body>::find_aabb() const {
	if (const auto body = find_body()) {
		if (const auto aabb = ::find_aabb(*body)) {
			const auto si = handle.get_cosmos().get_si();
			return ltrb(
				aabb->lowerBound.x,		
				aabb->lowerBound.y,		
				aabb->upperBound.x,		
				aabb->upperBound.x
			).to_user_space(si);
		}
	}

	return std::nullopt;
}
