#pragma once
#include "game/transcendental/entity_handle_declaration.h"

template <bool, class D>
class relations_mixin;

namespace augs {
	template <template <class T> class make_pool_id, class... components>
	class component_aggregate; 
}

class cosmic_delta;

class cosmos_solvable_access {
	/*
		The following domains are free to arbitrarily change the cosmos_solvable::significant,
		as they take proper precautions to keep state consistent or otherwise refresh it.
	*/

	/*
		Note that, additionally, all cosmos functons can mutate the solvable state.
		We might want to make some of the cosmos member functions free-standing so that they don't have write access automatically.
	*/

	friend cosmic_delta;

	template <bool is_const>
	friend class basic_entity_handle;

	template <bool, class D>
	friend class relations_mixin;

	cosmos_solvable_access() {}
};