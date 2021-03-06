#pragma once
#include "augs/math/vec2.h"

#include "game/enums/attitude_type.h"

#include "game/cosmos/entity_id.h"
#include "game/cosmos/entity_handle_declaration.h"

real32 assess_projectile_velocity_of_weapon(const const_entity_handle& weapon);

void unset_input_flags_of_orphaned_entity(const entity_handle&);

struct identified_danger {
	real32 amount = 0.f;
	entity_id danger;
	vec2 recommended_evasion;
};

identified_danger assess_danger(
	const const_entity_handle& victim, 
	const const_entity_handle& danger
);

attitude_type calc_attitude(
	const const_entity_handle targeter, 
	const const_entity_handle target
);

struct ammunition_information {
	unsigned total_charges = 0;
	real32 total_ammo_space = 0.f;
	real32 available_ammo_space = 0.f;

	real32 get_ammo_ratio() const;
};

ammunition_information calc_ammo_info(const const_entity_handle handle);
ammunition_information calc_reloadable_ammo_info(const const_entity_handle handle);

struct b2Filter;

entity_id get_closest_hostile(
	const const_entity_handle subject,
	const const_entity_handle subject_attitude,
	const float radius,
	const b2Filter filter
);

std::vector<entity_id> get_closest_hostiles(
	const const_entity_handle subject,
	const const_entity_handle subject_attitude,
	const real32 radius,
	const b2Filter filter
);