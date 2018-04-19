#pragma once
#include <type_traits>

#include "generated/introspectors.h"

#include "augs/templates/for_each_type.h"
#include "augs/templates/recursive.h"
#include "augs/templates/maybe_const.h"
#include "augs/templates/traits/container_traits.h"
#include "augs/templates/traits/introspection_traits.h"
#include "augs/templates/traits/enum_introspection_traits.h"
#include "augs/templates/traits/is_optional.h"
#include "augs/templates/traits/is_comparable.h"
#include "augs/templates/traits/is_tuple.h"

#include "augs/templates/introspect_declaration.h"

namespace augs {
	/*
		Simple introspection with just one level of depth.
		Will invoke a callback upon every top-level field of a struct,
		and also on all fields of base classes specified with either of these two:

			using introspect_base = ...;
			using introspect_bases = type_list<...>;
	*/

	template <class F, class Instance, class... Instances>
	void introspect_body(
		F&& callback,
		Instance& t,
		Instances&... tn
	) {
		using T = std::remove_reference_t<Instance>;
		static_assert(has_introspect_body_v<T>, "No introspector found for the type.");

		introspection_access::introspect_body(
			static_cast<std::decay_t<Instance>*>(nullptr), 
			std::forward<F>(callback), t, tn...
		);
	}

	template <class F, class Instance, class... Instances>
	void introspect(
		F&& callback,
		Instance& t,
		Instances&... tn
	) {
		using T = std::remove_reference_t<Instance>;
		static constexpr auto C = std::is_const_v<T>;

		if constexpr(has_introspect_base_v<T>) {
			introspect(
				std::forward<F>(callback), 
				static_cast<maybe_const_ref_t<C, typename T::introspect_base>>(t),
				tn...
			);
		}
		else if constexpr(has_introspect_bases_v<T>) {
			for_each_type_in_list<typename T::introspect_bases>([&](const auto& b){
				introspect(
					std::forward<F>(callback), 
					static_cast<maybe_const_ref_t<C, std::decay_t<decltype(b)>>>(t),
					tn...
				);
			});
		}

		static_assert(
			(!has_introspect_bases_v<T> && !has_introspect_bases_v<T>) || (has_introspect_bases_v<T> != has_introspect_base_v<T>),
		   	"Please choose only one way to specify introspected bases."
		);

		if constexpr(has_introspect_body_v<T>) {
			introspect_body(std::forward<F>(callback), t, tn...);
		}
		else {
			static_assert(
				has_introspect_bases_v<T> || has_introspect_base_v<T>,
			   	"No introspector found for the types, and no introspected bases were specified."
			);
		}
	}

	template <class A, class B>
	bool equal_or_introspective_equal(
		const A& a,
		const B& b
	) {
		if constexpr(is_optional_v<A>) {
			static_assert(is_optional_v<B>);

			if (a.has_value() != b.has_value()) {
				return false;
			}
			else if (a && b) {
				return equal_or_introspective_equal(*a, *b);
			}
			else {
				ensure(!a && !b);
				return true;
			}
		}
		else if constexpr(
			is_tuple_v<A>
			|| is_std_array_v<A>
		) {
			return introspective_equal(a, b);
		}
		else if constexpr(is_comparable_v<A, B>) {
			return a == b;
		}
		else {
			return introspective_equal(a, b);
		}
	}

	template <class A, class B>
	bool introspective_equal(const A& a, const B& b) {
		static_assert(has_introspect_v<A> && has_introspect_v<B>, "Comparison requested on type(s) without introspectors!");

		bool are_equal = true;

		introspect(
			[&are_equal](auto, const auto& aa, const auto& bb) {
				are_equal = are_equal && equal_or_introspective_equal(aa, bb);
			}, a, b
		);

		return are_equal;
	}
}