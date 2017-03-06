#pragma once
#include "game/transcendental/entity_id.h"
#include "game/enums/grenade_type.h"

namespace components {
	struct grenade {
		child_entity_id spoon;
		grenade_type type = grenade_type::INVALID;
	};
}
