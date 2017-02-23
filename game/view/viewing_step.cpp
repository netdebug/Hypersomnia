#include "viewing_step.h"
#include "game/transcendental/cosmos.h"

double viewing_step::get_interpolated_total_time_passed_in_seconds() const {
	return cosm.get_total_time_passed_in_seconds(get_interpolation_ratio());
}

float viewing_step::get_interpolation_ratio() const {
	return interpolation_ratio;
}

viewing_step::viewing_step(
	const config_lua_table& config,
	const cosmos& cosm,
	const viewing_session& session,
	const float interpolation_ratio,
	augs::renderer& renderer,
	const camera_cone camera,
	const entity_id viewed_character,
	const visible_entities& visible
) :
	const_cosmic_step(cosm),
	config(config),
	session(session),
	interpolation_ratio(interpolation_ratio),
	renderer(renderer),
	camera(camera),
	viewed_character(viewed_character),
	visible(visible)
{}