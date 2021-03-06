#pragma once
#include <vector>

#include "3rdparty/streflop/streflop.h"
#include "augs/misc/randomization_declaration.h"

#include "augs/misc/bound.h"
#include "augs/math/vec2.h"
#include "augs/templates/variated.h"

template <class generator_type>
struct basic_randomization {
	generator_type generator;

	basic_randomization(const rng_seed_type seed = 0);

	int randval(
		int min, 
		int max
	);

	int randval_v(
		int base_value, 
		int variation
	);

	unsigned randval(
		unsigned min, 
		unsigned max
	);

	std::size_t randval(
		std::size_t min, 
		std::size_t max
	);

	real32 randval(
		real32 min, 
		real32 max
	);

	real32 randval_v(
		real32 base_value, 
		real32 variation
	);

	real32 randval_vm(
		real32 base_value, 
		real32 variation_mult
	);

	template <class T>
	auto randval(const augs::variated<T>& v) {
		if constexpr(std::is_same_v<T, unsigned>) {
			return static_cast<unsigned>(randval_v(static_cast<int>(v.value), static_cast<int>(v.variation)));
		}
		else {
			return randval_v(v.value, v.variation);
		}
	}

	template <class T>
	auto randval(const augs::mult_variated<T>& v) {
		return randval_vm(v.value, v.variation);
	}

	real32 randval_h(real32 bound);
	int randval_h(int bound);
	
	std::vector<real32> make_random_intervals(
		const std::size_t n, 
		const real32 maximum
	);

	std::vector<real32> make_random_intervals(
		const std::size_t n,
		const real32 maximum, 
		const real32 variation_multiplier
	);

	template <class A, class B>
	auto randval(const augs::simple_pair<A, B> p) {
		return randval(p.first, p.second);
	}

	template<class T>
	auto randval(const augs::random_bound<T> r) {
		return augs::bound<T>(randval(r.first), randval(r.second));
	}

	template <class T>
	basic_vec2<T> random_point_on_unit_circle() {
		const auto random_angle = randval(static_cast<T>(0), static_cast<T>(360));
		return basic_vec2<T>::from_degrees(random_angle);
	}

	template<class T>
	basic_vec2<T> random_point_in_ring(
		const T min_radius,
		const T max_radius
	) {
		return randval(min_radius, max_radius) * random_point_on_unit_circle<T>();
	}

	template <class T>
	basic_vec2<T> random_point_in_unit_circle() {
		return random_point_in_ring(static_cast<T>(0), static_cast<T>(1));
	}

	template<class T>
	basic_vec2<T> random_point_in_circle(
		const T max_radius
	) {
		return random_point_in_ring(static_cast<T>(0), max_radius);
	}

	template<class T>
	basic_vec2<T> randval(
		const basic_vec2<T> min_a, 
		const basic_vec2<T> max_a
	) {
		return { 
			randval(min_a.x, max_a.x), 
			randval(min_a.y, max_a.y) 
		};
	}

	template <class C>
	auto choose_from(C& container) {
		return container[randval(static_cast<std::size_t>(0), container.size() - 1)];
	}
};

struct randomization : basic_randomization<streflop::RandomState> {
	using basic_randomization<streflop::RandomState>::basic_randomization;
};
