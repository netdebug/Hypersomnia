#pragma once
#include <iosfwd>
#include "3rdparty/sol2/sol/forward.hpp"

#include "augs/build_settings/platform_defines.h"

#include "augs/templates/maybe_const.h"
#include "augs/templates/component_traits.h"
#include "augs/templates/type_matching_and_indexing.h"

#include "game/detail/inventory/inventory_slot_handle_declaration.h"
#include "game/transcendental/entity_handle_declaration.h"
#include "game/transcendental/entity_id.h"
#include "game/organization/all_components_declaration.h"

#include "game/detail/entity_handle_mixins/inventory_mixin.h"
#include "game/detail/entity_handle_mixins/physics_mixin.h"
#include "game/detail/entity_handle_mixins/relations_mixin.h"
#include "game/detail/entity_handle_mixins/spatial_properties_mixin.h"

#include "game/enums/entity_flag.h"
#include "game/transcendental/step_declaration.h"
#include "game/components/type_component_declaration.h"
#include "game/components/flags_component.h"

class cosmos;
struct cosmic_delta;

template <bool, class>
class component_synchronizer;

namespace augs {
	void read_object_lua(sol::table ar, cosmos& cosm);
}

template <bool is_const>
class basic_entity_handle :
	public inventory_mixin<is_const, basic_entity_handle<is_const>>,
	public physics_mixin<is_const, basic_entity_handle<is_const>>,
	public relations_mixin<is_const, basic_entity_handle<is_const>>,
	public spatial_properties_mixin<is_const, basic_entity_handle<is_const>>
{
	/* For cloning */
	template <bool, class> friend class relations_mixin;
	template <bool, class> friend class basic_relations_mixin;

	template <bool> friend class basic_entity_handle;

	friend cosmic_delta;
	friend cosmos;

	using owner_reference = maybe_const_ref_t<is_const, cosmos>;
	using entity_ptr = maybe_const_ptr_t<is_const, cosmic_entity>;

	entity_ptr ptr;
	owner_reference owner;
	entity_id raw_id;

	template <typename T>
	static void check_component_type() {
		static_assert(is_one_of_list_v<T, component_list_t<type_list>>, "Unknown component type!");
	}

	auto& get() const {
		return *ptr;
	}

	auto& pool_provider() const {
		return owner.solvable;
	}

private:
	basic_entity_handle(
		owner_reference owner,
		const entity_id raw_id,
		const entity_ptr ptr
	) :
		raw_id(raw_id),
		owner(owner),
		ptr(ptr)
	{}

public:
	basic_entity_handle(
		owner_reference owner, 
		const entity_id raw_id
	) : basic_entity_handle(
		owner, 
		raw_id, 
		owner.solvable.get_entity_pool({}).find(raw_id)
	) {
	}

	entity_id get_id() const {
		return raw_id;
	}

	void set_id(const entity_id id) {
		raw_id = id;
	}

	bool alive() const {
		return ptr != nullptr;
	}

	bool dead() const {
		return !alive();
	}

	auto& get_cosmos() const {
		return owner;
	}

	bool operator==(const entity_id id) const {
		return raw_id == id;
	}

	bool operator!=(const entity_id id) const {
		return raw_id != id;
	}

	template <bool C = !is_const, class = std::enable_if_t<C>>
	operator const_entity_handle() const {
		return const_entity_handle(owner, raw_id, ptr);
	}

	operator entity_id() const {
		return raw_id;
	}

	operator child_entity_id() const {
		return raw_id;
	}

	operator unversioned_entity_id() const {
		return raw_id;
	}

	operator bool() const {
		return alive();
	}

	template <class component>
	bool has() const {
		return get().template has<component>(pool_provider());
	}

	template <class T>
	decltype(auto) get() const {
		check_component_type<T>();

		ensure(alive());
		
		if constexpr(is_component_synchronized_v<T>) {
			return component_synchronizer<is_const, T>(&get().template get<T>(pool_provider()), *this);
		}
		else {
			return get().template get<T>(pool_provider());
		}
	}

	template <class T, bool C = !is_const, class = std::enable_if_t<C>>
	void add(const T& c) const {
		check_component_type<T>();
		ensure(alive());
		
		if constexpr(is_component_synchronized_v<T>) {
			get().template add<T>(c, pool_provider());
			owner.reinfer_caches_of(*this);
		}
		else {
			get().template add<T>(c, pool_provider());
		}
	}

	template <class T, bool C = !is_const, class = std::enable_if_t<C>>
	void add(const component_synchronizer<is_const, T>& c) const {
		add(c.get_raw_component());
	}

	template<class T>
	decltype(auto) find() const {
		check_component_type<T>();

		ensure(alive());

		if constexpr(is_component_synchronized_v<T>) {
			return component_synchronizer<is_const, T>(get().template find<T>(pool_provider()), *this);
		}
		else {
			return get().template find<T>(pool_provider());
		}
	}

	template <class T, bool C = !is_const, class = std::enable_if_t<C>>
	void remove() const {
		check_component_type<T>();

		ensure(alive());

		if constexpr(is_component_synchronized_v<T>) {
			get().template remove<T>(pool_provider());
			owner.reinfer_caches_of(*this);
		}
		else {
			get().template remove<T>(pool_provider());
		}
	}

	template <class component, bool C = !is_const, class = std::enable_if_t<C>>
	decltype(auto) operator+=(const component& c) const {
		add(c);
		return get<component>();
	}

	template <bool C = !is_const, class = std::enable_if_t<C>>
	entity_handle add_standard_components(const logic_step step, const bool activate_inferred) const;

	template <bool C = !is_const, class = std::enable_if_t<C>>
	entity_handle add_standard_components(const logic_step step) const;

	template <bool C = !is_const, class = std::enable_if_t<C>>
	void recalculate_basic_processing_categories() const;

	bool get_flag(const entity_flag f) const {
		ensure(alive());
		components::flags from;

		if (has<components::flags>()) {
			from = get<components::flags>();
		}

		return from.values.test(f);
	}

	template <bool C = !is_const, class = std::enable_if_t<C>>
	void set_flag(const entity_flag f) const {
		ensure(alive());
		if (!has<components::flags>()) {
			add(components::flags());
		}

		get<components::flags>().values.set(f, true);
	}

	template <bool C = !is_const, class = std::enable_if_t<C>>
	void unset_flag(const entity_flag f) const {
		ensure(alive());
		if (!has<components::flags>()) {
			add(components::flags());
		}

		get<components::flags>().values.set(f, false);
	}

	template <class F>
	void for_each_component(F&& callback) const {
		ensure(alive());

		get().for_each_component(
			std::forward<F>(callback),
		   	pool_provider()
		);
	}
	
	entity_guid get_guid() const {
		return get_cosmos().solvable.get_guid(raw_id);
	}

	auto& get_type() const {
		return get<components::type>().get_type();
	}

	auto get_type_id() const {
		return get<components::type>().get_type_id();
	}

	const auto& get_name() const {
		return get<components::type>().get_name();
	}

	bool is_inferred_state_activated() const {
		const auto inferred = find<components::all_inferred_state>();

		return inferred != nullptr && inferred.is_activated();
	}
};

std::ostream& operator<<(std::ostream& out, const entity_handle&x);
std::ostream& operator<<(std::ostream& out, const const_entity_handle &x);

inline auto linear_cache_key(const const_entity_handle handle) {
	return linear_cache_key(handle.get_id());
}
