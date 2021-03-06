#include "augs/templates/container_templates.h"
#include "game/detail/physics/physics_queries.h"

#include "augs/misc/randomization.h"

#include "game/cosmos/cosmos.h"
#include "game/cosmos/entity_handle.h"
#include "game/cosmos/data_living_one_step.h"
#include "game/cosmos/for_each_entity.h"

#include "game/components/interpolation_component.h"
#include "game/components/fixtures_component.h"

#include "game/messages/start_particle_effect.h"

#include "view/viewables/all_viewables_declaration.h"
#include "view/viewables/particle_effect.h"
#include "view/viewables/particle_types.h"

#include "view/audiovisual_state/systems/particles_simulation_system.h"
#include "view/audiovisual_state/systems/interpolation_system.h"
#include "augs/templates/traits/is_nullopt.h"
#include "game/detail/gun/firearm_engine.h"

using emi_inst = particles_simulation_system::emission_instance;

bool emi_inst::is_over() const {
	return stream_lifetime_ms >= stream_max_lifetime_ms;
}

float emi_inst::calc_alivity_mult() const {
	if (stream_max_lifetime_ms == 0.f) {
		return 1.f;
	}

	return stream_lifetime_ms / stream_max_lifetime_ms;
}

float emi_inst::advance_lifetime_get_dt(const augs::delta& delta, const bool stream_infinitely) {
	const auto dt = delta.in_milliseconds();

	if (stream_infinitely) {
		stream_lifetime_ms += dt;
		return dt;
	}

	if (const auto remaining = stream_max_lifetime_ms - stream_lifetime_ms;
		dt >= remaining
	) {
		stream_lifetime_ms = stream_max_lifetime_ms;
		return remaining;
	}

	stream_lifetime_ms += dt;
	return dt;
}

void particles_simulation_system::emission_instance::init_bounds(
	const particles_emission& emission,
	randomization& rng
) {
	{
		const auto var_v = rng.randval(emission.base_speed_variation);
		particle_speed.set(std::max(0.f, emission.base_speed.first - var_v / 2), emission.base_speed.second + var_v / 2);
	}

	source_emission = emission;
	angular_offset = rng.randval(emission.angular_offset);
	spread = rng.randval(emission.spread_degrees);
	particles_per_sec = rng.randval(emission.particles_per_sec);
	swing_spread = rng.randval(emission.swing_spread);
	swings_per_sec = rng.randval(emission.swings_per_sec);

	swing_spread_bound = rng.randval(emission.swing_spread_bound);
	swings_per_sec_bound = rng.randval(emission.swings_per_sec_bound);

	stream_max_lifetime_ms = rng.randval(emission.stream_lifetime_ms);
	stream_particles_to_spawn = rng.randval(emission.num_of_particles_to_spawn_initially);
	swing_speed_change = rng.randval(emission.swing_speed_change_rate);
	swing_spread_change = rng.randval(emission.swing_spread_change_rate);

	fade_when_ms_remaining = rng.randval(emission.fade_when_ms_remaining);

	randomize_spawn_point_within_circle_of_inner_radius = rng.randval(emission.randomize_spawn_point_within_circle_of_inner_radius);
	randomize_spawn_point_within_circle_of_outer_radius = rng.randval(emission.randomize_spawn_point_within_circle_of_outer_radius);

	starting_homing_force = rng.randval(emission.starting_homing_force);
	ending_homing_force = rng.randval(emission.ending_homing_force);

	starting_spawn_circle_size_multiplier = rng.randval(emission.starting_spawn_circle_size_multiplier);
	ending_spawn_circle_size_multiplier = rng.randval(emission.ending_spawn_circle_size_multiplier);
}

particles_simulation_system::emission_instance::emission_instance(
	const particles_emission& emission,
	randomization& rng
) {
	init_bounds(emission, rng);
}

particles_simulation_system::basic_cache::basic_cache(
	const packaged_particle_effect& original,
	const particle_effects_map& manager,
	randomization& rng
) : original(original) {
	if (const auto* const source_effect = mapped_or_nullptr(manager, original.input.id)) {
		emission_instances.reserve(source_effect->emissions.size());

		for (auto emission : source_effect->emissions) {
			if (container_full(emission_instances)) {
				break;
			}

			emission_instances.emplace_back(emission, rng);
			auto& e = emission_instances.back();
			e.stream_particles_to_spawn *= original.input.modifier.scale_amounts;
		}
	}
	else {
		throw effect_not_found {};
	}
}

std::size_t particles_simulation_system::count_all_particles() const {
	std::size_t total = 0;

	total += ::accumulate_sizes(general_particles);
	total += ::accumulate_sizes(animated_particles);

	for (const auto& m : homing_animated_particles) {
		for (const auto& v : m) {
			total += v.second.size();
		}
	}

	return total;
}

void particles_simulation_system::clear() {
	orbital_emissions.clear();
	fire_and_forget_emissions.clear();

	auto clearer = [](auto& arr) { for(auto& v : arr) { v.clear(); } };

	clearer(general_particles);
	clearer(animated_particles);
	clearer(homing_animated_particles);
}

void particles_simulation_system::add_particle(const particle_layer l, const general_particle& p) {
	auto& v = general_particles[l];

	if (container_full(v)) {
		return;
	}

	v.push_back(p);
}

void particles_simulation_system::add_particle(const particle_layer l, const animated_particle& p) {
	auto& v = animated_particles[l];

	if (container_full(v)) {
		return;
	}

	v.push_back(p);
}

void particles_simulation_system::add_particle(const particle_layer l, const entity_id id, const homing_animated_particle& p) {
	homing_animated_particles[l][id].push_back(p);
}

void particles_simulation_system::update_effects_from_messages(
	randomization& rng,
	const const_logic_step step,
	const particle_effects_map& manager,
	const interpolation_system& 
) {
	{
		const auto& events = step.get_queue<messages::stop_particle_effect>();

		for (auto& e : events) {
			erase_if(orbital_emissions, [&e](const orbital_cache& c){	
				if (const auto& m = e.match_chased_subject) {
					if (*m != c.chasing.target) {
						return false;
					}	
				}

				if (const auto m = e.match_orbit_offset) {
					if (*m != c.chasing.offset.pos) {
						return false;
					}	
				}

				if (const auto m = e.match_effect_id) {
					if (*m != c.original.input.id) {
						return false;
					}	
				}

				return true;
			});
		}
	}

	const auto& events = step.get_queue<messages::start_particle_effect>();

	for (auto& e : events) {
		const auto& start = e.payload.start;

		try {
			if (!start.positioning.target.is_set()) {
				if (!container_full(fire_and_forget_emissions)) {
					fire_and_forget_emissions.emplace_back(
						start.positioning.offset,

						e.payload,
						manager,
						rng
					);
				}
			}
			else {
				if (!container_full(orbital_emissions)) {
					orbital_emissions.emplace_back(
						start.positioning,

						e.payload,
						manager,
						rng
					);
				}
			}
		}
		catch (const effect_not_found&) {

		}
	}
}

void particles_simulation_system::integrate_all_particles(
	const cosmos& cosm,
	const augs::delta delta,
	const plain_animations_pool& anims,
	const interpolation_system& interp
) {
	const auto dead_particles_remover = [](auto& container) {
		erase_if(container, [](const auto& a) { return a.is_dead(); });
	};

	for (auto& particle_layer : general_particles) {
		dead_particles_remover(particle_layer);

		for (auto& p : particle_layer) {
			p.integrate(delta.in_seconds());
		}
	}

	for (auto& particle_layer : animated_particles) {
		dead_particles_remover(particle_layer);

		for (auto& p : particle_layer) {
			p.integrate(delta.in_seconds(), anims);
		}
	}

	for (auto& particle_layer : homing_animated_particles) {
		erase_if(particle_layer, [&](auto& cluster) {
			const auto homing_target = cosm[cluster.first];

			if (homing_target.alive()) {
				dead_particles_remover(cluster.second);

				const auto homing_transform = cosm[cluster.first].get_viewing_transform(interp);

				for (auto& p : cluster.second) {
					p.integrate(delta.in_seconds(), homing_transform.pos, anims);
				}

				return cluster.second.empty();
			}

			return true;
		});
	}
}

template <class Component, class Caches, class EffectProvider>
void update_component_related_cache(
	randomization& rng,
	const particle_effects_map& manager,
	const cosmos& cosm,
	Caches& caches,
	EffectProvider effect_provider
) {
	cosm.for_each_having<Component>(
		[&](const auto& typed_handle) {
			const auto id = typed_handle.get_id().to_unversioned();

			if (const auto item = typed_handle.template find<components::item>()) {
				if (const auto slot = typed_handle.get_current_slot(); slot.alive()) {
					/* 
						Disable effects for holstered items and items on the body,
						to prevent giving away of the positions.
					*/

					if (!slot.is_hand_slot()) {
						caches.erase(id);
						return;
					}

					if (typed_handle.find_colliders_connection() == std::nullopt) {
						caches.erase(id);
						return;
					}
				}
			}

			if (const auto particles = effect_provider(typed_handle)) {
				if (auto* const existing = mapped_or_nullptr(caches, id)) {
					existing->cache.original = *particles;
				}
				else {
					try {
						caches.try_emplace(id, particles_simulation_system::continuous_particles_cache { { particles->start.positioning, *particles, manager, rng }, { typed_handle.get_name() } });
					}
					catch (const particles_simulation_system::effect_not_found&) {

					}
				}
			}
			else {
				caches.erase(id);
			}
		}
	);

	erase_if(caches, [&](auto& it) {
		return cosm[it.first].dead();
	});
}

void particles_simulation_system::advance_visible_streams(
	randomization& rng,
	const camera_cone current_cone, 
	const cosmos& cosm,
	const particle_effects_map& manager,
	const plain_animations_pool& anims,
	const augs::delta& delta,
	const interpolation_system& interp
) {
	const auto dt_secs = delta.in_seconds();

	auto advance_emissions = [&](
		emission_instances_type& instances, 
		const transformr current_transform,
		const bool visible_in_camera,
		const packaged_particle_effect& effect
	) {
		const auto& modifier = effect.input.modifier;
		const auto homing_target = effect.start.homing_target;
		const auto infinitely = effect.start.stream_infinitely;

		for (auto& instance : instances) {
			const auto stream_alivity_mult = infinitely ? 1.f : instance.calc_alivity_mult();
			const auto stream_delta = instance.advance_lifetime_get_dt(delta, infinitely);

			if (!visible_in_camera) {
				continue;
			}

			auto new_particles_to_spawn_by_time = 
				(instance.particles_per_sec * modifier.scale_amounts) * 
				(stream_delta / 1000.f)
			;

			instance.stream_particles_to_spawn += new_particles_to_spawn_by_time;

			instance.swings_per_sec += rng.randval_h(instance.swing_speed_change);
			instance.swing_spread += rng.randval_h(instance.swing_spread_change);

			auto clamp = [](auto& val, const auto bound) {
				val = std::clamp(val, bound.first, bound.second);
			};

			if (instance.swing_spread_bound.second > 0) {
				clamp(instance.swing_spread, instance.swing_spread_bound);
			}
			if (instance.swings_per_sec_bound.second > 0) {
				clamp(instance.swings_per_sec, instance.swings_per_sec_bound);
			}

#if TODO
			const auto segment_length = existence.distribute_within_segment_of_length;
#endif
			const float segment_length = 0.f;

			const vec2 segment_A = current_transform.pos + vec2::from_degrees(current_transform.rotation + 90).set_length(segment_length / 2);
			const vec2 segment_B = current_transform.pos - vec2::from_degrees(current_transform.rotation + 90).set_length(segment_length / 2);
			
			const auto homing_target_pos = cosm[homing_target].alive() ? cosm[homing_target].get_viewing_transform(interp).pos : vec2();

			const auto to_spawn = static_cast<int>(instance.stream_particles_to_spawn);

			for (int i = 0; i < to_spawn; ++i) {
				const float t = (static_cast<float>(i) / to_spawn);
				const float time_elapsed = (1.f - t) * dt_secs;

				vec2 final_particle_position = augs::interp(segment_A, segment_B, rng.randval(0.f, 1.f));
				
				if (
					instance.randomize_spawn_point_within_circle_of_inner_radius > 0.f
					|| instance.randomize_spawn_point_within_circle_of_outer_radius > 0.f
				) {
					const auto size_mult = augs::interp(
						instance.starting_spawn_circle_size_multiplier,
						instance.ending_spawn_circle_size_multiplier,
						stream_alivity_mult
					);
					
					final_particle_position += rng.random_point_in_ring(
						size_mult * instance.randomize_spawn_point_within_circle_of_inner_radius,
						size_mult * instance.randomize_spawn_point_within_circle_of_outer_radius
					);
				}

				const auto& emission = instance.source_emission;

				/* MSVC ICE workaround */
				auto& _rng = rng;

				const auto spawner = [&](auto dummy) {
					using spawned_particle_type = decltype(dummy);

					auto particle = this->spawn_particle<spawned_particle_type>(
						_rng,
						instance.angular_offset,
						instance.particle_speed,
						final_particle_position,
						current_transform.rotation + instance.swing_spread * static_cast<float>(std::sin((instance.stream_lifetime_ms / 1000.f) * 2 * PI<float> * instance.swings_per_sec)),
						instance.spread,
						emission
					);

					return ::apply_to_particle(modifier, particle);
				};

				if (emission.has<general_particle>()) {
					auto new_general = spawner(general_particle());
					new_general.integrate(time_elapsed);
					add_particle(emission.target_layer, new_general);
				}

				if (emission.has<animated_particle>()) {
					auto new_animated = spawner(animated_particle());
					new_animated.integrate(time_elapsed, anims);
					add_particle(emission.target_layer, new_animated);
				}

				if (emission.has<homing_animated_particle>()) {
					auto new_homing_animated = spawner(homing_animated_particle());

					new_homing_animated.homing_force = augs::interp(
						instance.starting_homing_force,
						instance.ending_homing_force,
						stream_alivity_mult
					);

					new_homing_animated.integrate(time_elapsed, homing_target_pos, anims);
					add_particle(emission.target_layer, homing_target, new_homing_animated);
				}
			}

			/* Leave only the fractional part */
			instance.stream_particles_to_spawn -= static_cast<int>(instance.stream_particles_to_spawn);
		}
	};

	{
		auto checked_cone = current_cone;
		checked_cone.eye.zoom /= 2.5f;

		const auto cam_aabb = checked_cone.get_visible_world_rect_aabb();

		erase_if(fire_and_forget_emissions, [&](auto& c) {
			const auto where = c.transform;
			const bool visible_in_camera = cam_aabb.hover(where.pos);

			advance_emissions(c.emission_instances, where, visible_in_camera, c.original);
			return c.is_over();
		});

		erase_if(orbital_emissions, [&](auto& c) {
			const auto chase = c.chasing;
			const auto where = find_transform(chase, cosm, interp);
			
			if (where == std::nullopt) {
				return true;
			}

			const bool visible_in_camera = cam_aabb.hover(where->pos);

			advance_emissions(c.emission_instances, *where, visible_in_camera, c.original);
			return c.is_over();
		});

		erase_if(firearm_engine_caches, [&](auto& it) {
			auto& c = it.second.cache;

			const auto chase = c.chasing;
			const auto where = find_transform(chase, cosm, interp);

			if (where == std::nullopt) {
				return true;
			}

			if (const auto subject = cosm[it.first]) {
				if (subject.get_name() != it.second.recorded.name) {
					return true;
				}
			}

			const bool visible_in_camera = cam_aabb.hover(where->pos);

			advance_emissions(c.emission_instances, *where, visible_in_camera, c.original);
			return false;
		});
		
		erase_if(continuous_particles_caches, [&](auto& it) { 
			auto& c = it.second.cache;
			const auto chase = c.chasing;

			auto where = find_transform(chase, cosm, interp);

			if (where == std::nullopt) {
				return true;
			}

			if (const auto subject = cosm[it.first]) {
				if (subject.get_name() != it.second.recorded.name) {
					return true;
				}
			}

			const auto displacement = [&]() {
				if (const auto subject = cosm[it.first]) {
					return subject.template dispatch_on_having_all_ret<components::continuous_particles>([](const auto& typed_subject) {
						if constexpr(is_nullopt_v<decltype(typed_subject)>) {
							return transformr();
						}
						else {
							return transformr(typed_subject.template get<components::continuous_particles>().wandering_state.current, 0);
						}
					});
				}

				return transformr();
			}();

			*where += displacement;
			const bool visible_in_camera = cam_aabb.hover(where->pos);

			advance_emissions(c.emission_instances, *where, visible_in_camera, c.original);
			return false;
		});
	}

	update_component_related_cache<components::gun>(
		rng,
		manager,
		cosm,
		firearm_engine_caches,
		[](const auto h) {
			return ::calc_firearm_engine_particles(h);
		}
	);

	update_component_related_cache<components::continuous_particles>(
		rng,
		manager,
		cosm,
		continuous_particles_caches,
		[](const auto h) -> std::optional<packaged_particle_effect> {
			const auto& continuous_particles = h.template get<invariants::continuous_particles>();

			packaged_particle_effect particles;

			particles.start = particle_effect_start_input::at_entity(h);
			particles.start.stream_infinitely = true;

			particles.input = continuous_particles.effect;

			if (!particles.input.id.is_set()) {
				return std::nullopt;
			}

			return std::make_optional(particles);
		}
	);
}
