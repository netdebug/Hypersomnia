#pragma once
#include "augs/misc/constant_size_vector.h"
#include "augs/misc/pool/pool.h"
#include "game/transcendental/cosmos_common_state.h"
#include "game/transcendental/cosmic_types.h"

using dynamic_component_pools_type = 
	replace_list_type_t<
		transform_types_in_list_t<
			cosmic_entity::dynamic_components_list,
			cosmic_object_pool
		>, 
		std::tuple
	>
;

using entity_pool_type = cosmic_object_pool<cosmic_entity>;

class cosmos_solvable_significant {
	// GEN INTROSPECTOR class cosmos_solvable_significant
	friend class cosmos_solvable_state;
	friend class cosmic_delta;
	friend augs::introspection_access;

	entity_pool_type entity_pool;
	dynamic_component_pools_type component_pools;
public:
	cosmos_common_state common;
	cosmos_meta meta;
	// END GEN INTROSPECTOR

	void clear();
};