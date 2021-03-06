#include "augs/math/camera_cone.h"
#include "augs/math/matrix.h"

#include "augs/drawing/drawing.h"

#include "augs/graphics/renderer.h"
#include "augs/graphics/shader.h"

#include "game/cosmos/entity_handle.h"
#include "game/cosmos/cosmos.h"
#include "game/cosmos/for_each_entity.h"
#include "game/enums/filters.h"

#include "view/frame_profiler.h"
#include "view/rendering_scripts/draw_entity.h"

#include "game/components/item_slot_transfers_component.h"
#include "game/debug_drawing_settings.h"
#include "game/detail/visible_entities.h"

#include "game/components/interpolation_component.h"
#include "game/components/fixtures_component.h"

#include "game/detail/inventory/inventory_slot_handle.h"

#include "view/rendering_scripts/rendering_scripts.h"
#include "view/rendering_scripts/illuminated_rendering.h"
#include "view/rendering_scripts/draw_wandering_pixels_as_sprites.h"
#include "view/rendering_scripts/helper_drawer.h"
#include "view/rendering_scripts/draw_area_indicator.h"

#include "view/viewables/all_viewables_declaration.h"
#include "view/viewables/image_in_atlas.h"
#include "view/viewables/images_in_atlas_map.h"
#include "game/stateless_systems/visibility_system.h"

#include "view/audiovisual_state/audiovisual_state.h"

#include "view/viewables/atlas_distributions.h"

#include "view/rendering_scripts/illuminated_rendering.h"
#include "game/detail/crosshair_math.hpp"

void illuminated_rendering(
	const illuminated_rendering_input in,
	const std::vector<additional_highlight>& additional_highlights
) {
	auto& profiler = in.frame_performance;

	auto& renderer = in.renderer;
	
	const auto viewed_character = in.camera.viewed_character;

	const auto cone = [&in](){ 
		auto result = in.camera.cone;
		result.eye.transform.pos.discard_fract();
		return result;
	}();

	const auto screen_size = cone.screen_size;

	const auto& cosm = viewed_character.get_cosmos();
	
	const auto& anims = cosm.get_logical_assets().plain_animations;

	const auto& av = in.audiovisuals;
	const auto& interp = av.get<interpolation_system>();
	const auto& particles = av.get<particles_simulation_system>();
	const auto& wandering_pixels = av.get<wandering_pixels_system>();
	const auto& exploding_rings = av.get<exploding_ring_system>();
	const auto& flying_numbers = av.get<flying_number_indicator_system>();
	const auto& highlights = av.get<pure_color_highlight_system>();
	const auto& thunders = av.get<thunder_system>();
	const auto global_time_seconds = cosm.get_total_seconds_passed(in.interpolation_ratio);
	const auto settings = in.drawing;
	const auto matrix = cone.get_projection_matrix();

	const bool is_zoomed_out = cone.eye.zoom < 1.f;

	auto non_zoomed_cone = cone;
	non_zoomed_cone.eye.zoom = 1.f;
	non_zoomed_cone.eye.transform.pos = screen_size / 2;

	const auto non_zoomed_matrix = non_zoomed_cone.get_projection_matrix();

	const auto& visible = in.all_visible;
	const auto& shaders = in.shaders;
	auto& fbos = in.fbos;
	const auto& necessarys = in.necessary_images;
	const auto& game_images = in.game_images;
	const auto blank = necessarys.at(assets::necessary_image_id::BLANK);
	const auto& gui_font = in.gui_font;

	const auto output = augs::drawer_with_default{ renderer.get_triangle_buffer(), blank };
	const auto line_output = augs::line_drawer_with_default{ renderer.get_line_buffer(), blank };
	const auto viewed_character_transform = viewed_character ? viewed_character.find_viewing_transform(interp) : std::optional<transformr>();

	const auto filtering = renderer.get_current_settings().default_filtering;

	auto bind_and_set_filter = [&](auto& tex) {
		tex.bind();
		tex.set_filtering(filtering);
	};

	if (in.general_atlas) {
		bind_and_set_filter(*in.general_atlas);
	}

	auto set_shader_with_matrix = [&](auto& shader) {
		shader->set_as_current();
		shader->set_projection(matrix);
	};

	auto set_shader_with_non_zoomed_matrix = [&](auto& shader) {
		shader->set_as_current();
		shader->set_projection(non_zoomed_matrix);
	};

	set_shader_with_matrix(shaders.standard);

	fbos.smoke->set_as_current();

	renderer.clear_current_fbo();
	renderer.set_additive_blending();
	
	const auto draw_particles_in = draw_particles_input{ output, false };

	auto total_particles_scope = measure_scope_additive(profiler.particles_rendering);
	auto total_layer_scope = measure_scope_additive(profiler.drawing_layers);

	auto draw_particles = [&](const particle_layer layer) {
		auto scope = measure_scope(total_particles_scope);

		particles.draw_particles(
			game_images,
			anims,
			draw_particles_in,
			layer
		);
	};

	draw_particles(particle_layer::DIM_SMOKES);
	draw_particles(particle_layer::ILLUMINATING_SMOKES);

	renderer.call_and_clear_triangles();

	fbos.illuminating_smoke->set_as_current();
	renderer.clear_current_fbo();

	draw_particles(particle_layer::ILLUMINATING_SMOKES);

	renderer.call_and_clear_triangles();
	
	renderer.set_standard_blending();

	augs::graphics::fbo::set_current_to_none();

	const auto& light = av.get<light_system>();
	
	const auto laser_glow = necessarys.at(assets::necessary_image_id::LASER_GLOW);
	const auto glow_edge_tex = necessarys.at(assets::necessary_image_id::LASER_GLOW_EDGE);

	const auto cast_highlight = necessarys.at(assets::necessary_image_id::CAST_HIGHLIGHT);

	const auto queried_cone = [&]() {
		auto c = cone;
		c.eye.zoom /= in.camera_query_mult;
		return c;
	}();

	const auto drawing_input = draw_renderable_input { 
		{
			output, 
			game_images, 
			global_time_seconds,
			flip_flags(),
			av.randomizing,
			queried_cone
		},
		interp
	};

	const bool fog_of_war_effective = 
		viewed_character_transform != std::nullopt 
		&& settings.fog_of_war.is_enabled()
	;

#if BUILD_STENCIL_BUFFER
	/* 
		We need a separate TLS because the one returned by thread_local_visibility_responses
		would get overwritten by light calculations before the call to fill_stencil.
	*/

	thread_local visibility_responses viewed_visibility;

	if (fog_of_war_effective) {
		const auto fow_size = settings.fog_of_war.get_real_size();

		auto fow_raycasts_scope = cosm.measure_raycasts(profiler.fog_of_war_raycasts);

		messages::visibility_information_request request;
		request.eye_transform = *viewed_character_transform;
		request.filter = predefined_queries::line_of_sight();
		request.queried_rect = fow_size;
		request.subject = viewed_character;

		auto& requests = thread_local_visibility_requests();
		requests.clear();
		requests.push_back(request);

		visibility_system(DEBUG_LOGIC_STEP_LINES).calc_visibility(cosm, requests, viewed_visibility);
	}

	auto fill_stencil = [&]() {
		if (viewed_visibility.size() > 0 && !viewed_visibility[0].empty()) {
			renderer.enable_stencil();

			renderer.start_writing_stencil();
			renderer.set_clear_color(rgba(0, 0, 0, 0));
			renderer.clear_stencil();

			const auto eye_pos = viewed_character_transform->pos;
			const auto& r = viewed_visibility[0];

			for (std::size_t t = 0; t < r.get_num_triangles(); ++t) {
				const auto world_light_tri = r.get_world_triangle(t, eye_pos);
				augs::vertex_triangle renderable_light_tri;

				renderable_light_tri.vertices[0].pos = world_light_tri[0];
				renderable_light_tri.vertices[1].pos = world_light_tri[1];
				renderable_light_tri.vertices[2].pos = world_light_tri[2];

				renderable_light_tri.vertices[0].color = white;
				renderable_light_tri.vertices[1].color = white;
				renderable_light_tri.vertices[2].color = white;

				renderer.push_triangle(renderable_light_tri);
			}

			const auto& angle = settings.fog_of_war.angle;

			if (angle >= 360.f) {
				set_shader_with_matrix(shaders.pure_color_highlight);
			}
			else {
				set_shader_with_matrix(shaders.fog_of_war);

				auto dir = calc_crosshair_displacement(viewed_character);

				if (dir.is_zero()) {
					dir.set(1, 0);
				}

				const auto left_dir = vec2(dir).rotate(-angle / 2).neg_y();
				const auto right_dir = vec2(dir).rotate(angle / 2).neg_y();

				const auto eye_frag_pos = [&]() {
					auto screen_space = cone.to_screen_space(eye_pos);	
					screen_space.y = screen_size.y - screen_space.y;
					return screen_space;
				}();

				shaders.fog_of_war->set_uniform("startingAngleVec", left_dir);
				shaders.fog_of_war->set_uniform("endingAngleVec", right_dir);
				shaders.fog_of_war->set_uniform("eye_frag_pos", eye_frag_pos);
			}

			renderer.call_and_clear_triangles();
			renderer.start_testing_stencil();
		}
	};

	if (fog_of_war_effective) {
		renderer.call_and_clear_triangles();
		fill_stencil();
		renderer.disable_stencil();
	}
#else
	auto fill_stencil = [&]() {

	};
#endif

	light.render_all_lights({
		renderer,
		profiler,
		total_layer_scope,
		cosm, 
		matrix,
		fbos.light.value(),
		*shaders.light, 
		*shaders.textured_light, 
		*shaders.standard, 
		[&]() {
			if (viewed_character) {
				draw_crosshair_lasers({
					[&](const vec2 from, const vec2 to, const rgba col) {
						if (!settings.draw_weapon_laser) {
							return;
						}

						const vec2 edge_size = static_cast<vec2>(glow_edge_tex.get_original_size());

						output.line(laser_glow, from, to, edge_size.y / 3.f, col);

						const auto edge_dir = (to - from).normalize();
						const auto edge_offset = edge_dir * edge_size.x;

						output.line(glow_edge_tex, to, to + edge_offset, edge_size.y / 3.f, col);
						output.line(glow_edge_tex, from - edge_offset + edge_dir, from + edge_dir, edge_size.y / 3.f, col, flip_flags::make_horizontally());
					},
					[](const vec2, const vec2, const rgba) {},
					interp,
					viewed_character
				});
			}

			draw_explosion_body_highlights({
				output,
				interp,
				cosm,
				global_time_seconds,
				cast_highlight
			});

			draw_beep_lights({
				output,
				interp,
				cosm,
				cast_highlight
			})();

			renderer.set_active_texture(3);
			bind_and_set_filter(fbos.illuminating_smoke->get_texture());
			renderer.set_active_texture(0);

			shaders.illuminating_smoke->set_as_current();

			renderer.fullscreen_quad();

			shaders.standard->set_as_current();

			exploding_rings.draw_highlights_of_rings(
				output,
				cast_highlight,
				cone
			);
		},
		fill_stencil,
		cone,
		fog_of_war_effective ? viewed_character : std::optional<entity_id>(),
		in.camera_query_mult,
		particles,
		anims,
		visible,
		cast_highlight,
		drawing_input
	});

	set_shader_with_matrix(shaders.illuminated);

	const auto helper = helper_drawer {
		visible,
		cosm,
		drawing_input,
		total_layer_scope
	};

	helper.draw<
		render_layer::UNDER_GROUND,
		render_layer::GROUND,
		render_layer::FLOOR_AND_ROAD,
		render_layer::ON_FLOOR,
		render_layer::ON_ON_FLOOR,

		render_layer::PLANTED_BOMBS,

		render_layer::AQUARIUM_FLOWERS,
		render_layer::AQUARIUM_DUNES,
		render_layer::BOTTOM_FISH,
		render_layer::UPPER_FISH,
		render_layer::AQUARIUM_BUBBLES
	>();

	visible.for_each<render_layer::DIM_WANDERING_PIXELS>(cosm, [&](const auto e) {
		draw_wandering_pixels_as_sprites(wandering_pixels, e, game_images, drawing_input.make_input_for<invariants::sprite>());
	});

	renderer.call_and_clear_triangles();

	set_shader_with_matrix(shaders.specular_highlights);

	renderer.call_and_clear_triangles();

	shaders.illuminated->set_as_current();

	helper.draw<
		render_layer::WATER_COLOR_OVERLAYS,
		render_layer::WATER_SURFACES,
		render_layer::CAR_INTERIOR,
		render_layer::CAR_WHEEL
	>();

	renderer.call_and_clear_triangles();

	set_shader_with_matrix(shaders.pure_color_highlight);

	const auto timestamp_ms = static_cast<unsigned>(global_time_seconds * 1000);
	
	auto standard_border_provider = [timestamp_ms](const const_entity_handle sentience) {
		std::optional<rgba> result;

		sentience.dispatch_on_having_all<components::sentience>(
			[&](const auto typed_handle) {
				result = typed_handle.template get<components::sentience>().find_low_health_border(timestamp_ms);
			}
		);

		return result;
	};

#if BUILD_STENCIL_BUFFER
	if (fog_of_war_effective) {
		renderer.call_and_clear_triangles();

		renderer.enable_stencil();
		renderer.stencil_positive_test();

		helper.visible.for_each<render_layer::SENTIENCES>(cosm, [&](const auto handle) {
			if (handle.get_official_faction() != viewed_character.get_official_faction()) {
				::draw_border(handle, drawing_input, standard_border_provider);
			}
		});

		renderer.call_and_clear_triangles();
		renderer.disable_stencil();

		helper.visible.for_each<render_layer::SENTIENCES>(cosm, [&](const auto handle) {
			if (handle.get_official_faction() == viewed_character.get_official_faction()) {
				::draw_border(handle, drawing_input, standard_border_provider);
			}
		});
	}
	else {
		helper.draw_border<render_layer::SENTIENCES>(standard_border_provider);
	}
#else
	helper.draw_border<render_layer::SENTIENCES>(standard_border_provider);
#endif

	renderer.call_and_clear_triangles();

	{
		const auto& markers = settings.draw_area_markers;

		if (markers.is_enabled) {
			visible.for_each<render_layer::AREA_MARKERS>(cosm, [&](const auto e) {
				e.template dispatch_on_having_all<invariants::box_marker>([&](const auto typed_handle) { 
					const auto where = typed_handle.get_logic_transform();
					const auto& marker_alpha = markers.value;
					::draw_area_indicator(typed_handle, line_output, where, cone.eye.zoom, marker_alpha, drawn_indicator_type::INGAME);
				});
			});
		}
	}

	if (viewed_character) {
		if (const auto sentience = viewed_character.find<components::sentience>()) {
			if (const auto sentience_def = viewed_character.find<invariants::sentience>()) {
				if (sentience->use_button != use_button_state::IDLE) {
					if (const auto tr = viewed_character.find_viewing_transform(interp)) {
						const auto& a = sentience_def->use_button_angle;
						const auto& r = sentience_def->use_button_radius;

						const bool is_querying = sentience->use_button == use_button_state::QUERYING;
						auto col = is_querying ? gray : rgba(255, 0, 0, 180);

						if (is_querying && sentience->last_use_result == use_button_query_result::IN_RANGE_BUT_CANT) {
							col = green;
						}

						if (r > 0.f) {
							const auto& p = tr->pos;
							const auto dash_len = 5.f;

							line_output.dashed_circular_sector(
								p,
								r,
								col,
								tr->rotation,
								a,
								dash_len
							);
						}
					}
				}
			}
		}
	}

	renderer.call_and_clear_lines();

	shaders.illuminated->set_as_current();

	helper.draw<
		render_layer::DYNAMIC_BODY,
		render_layer::OVER_DYNAMIC_BODY,
		render_layer::SMALL_DYNAMIC_BODY,
		render_layer::OVER_SMALL_DYNAMIC_BODY,
		render_layer::GLASS_BODY
	>();
	

#if BUILD_STENCIL_BUFFER
	if (fog_of_war_effective) {
		const auto& appearance = settings.fog_of_war_appearance;

		renderer.call_and_clear_triangles();
		renderer.enable_stencil();

		if (appearance.overlay_color_on_visible) {
			renderer.stencil_positive_test();
		}
		else {
			renderer.stencil_reverse_test();
		}

		set_shader_with_matrix(shaders.pure_color_highlight);

		const auto fow_size = settings.fog_of_war.get_real_size();

		output.aabb(
			ltrb::center_and_size(viewed_character_transform->pos, fow_size),
			appearance.overlay_color
		);

		renderer.call_and_clear_triangles();
		shaders.pure_color_highlight->set_projection(matrix);

		renderer.stencil_positive_test();

		shaders.illuminated->set_as_current();

		helper.visible.for_each<render_layer::SENTIENCES>(cosm, [&](const auto handle) {
			if (handle.get_official_faction() != viewed_character.get_official_faction()) {
				::draw_entity(handle, drawing_input);
			}
		});

		renderer.call_and_clear_triangles();
		renderer.disable_stencil();

		helper.visible.for_each<render_layer::SENTIENCES>(cosm, [&](const auto handle) {
			if (handle.get_official_faction() == viewed_character.get_official_faction()) {
				::draw_entity(handle, drawing_input);
			}
		});
	}
	else {
		helper.draw<render_layer::SENTIENCES>();
	}
#else
	helper.draw<render_layer::SENTIENCES>();
#endif

	renderer.call_and_clear_triangles();

	renderer.set_active_texture(1);

	bind_and_set_filter(fbos.smoke->get_texture());
	renderer.set_active_texture(0);

	shaders.smoke->set_as_current();

	renderer.fullscreen_quad();

	shaders.standard->set_as_current();
	
	helper.draw<
		render_layer::FLYING_BULLETS,
		render_layer::NEON_CAPTIONS
	>();
	
	if (settings.draw_crosshairs) {
		auto draw_crosshair = [&](const auto it) {
			if (const auto s = it.find_crosshair_def()) {
				auto in = drawing_input.make_input_for<invariants::sprite>();

				in.global_time_seconds = global_time_seconds;
				in.renderable_transform = it.get_world_crosshair_transform(interp);

				if (is_zoomed_out) {
					in.renderable_transform.pos = cone.to_screen_space(in.renderable_transform.pos);
				}

				augs::draw(s->appearance, game_images, in);
			}
		};

		if (viewed_character) {
			if (is_zoomed_out) {
				renderer.call_and_clear_triangles();
				shaders.standard->set_projection(non_zoomed_matrix);
			}

			draw_crosshair(viewed_character);

			if (is_zoomed_out) {
				renderer.call_and_clear_triangles();
				shaders.standard->set_projection(matrix);
			}
		}
	}
	
	if (settings.draw_weapon_laser && viewed_character.alive()) {
		const auto laser = necessarys.at(assets::necessary_image_id::LASER);
		
		draw_crosshair_lasers({
			line_output_wrapper { line_output, laser },
			dashed_line_output_wrapper  { line_output, laser, 10.f, 40.f, global_time_seconds },
			interp, 
			viewed_character
		});

		renderer.call_and_clear_lines();
	}

	draw_particles(particle_layer::ILLUMINATING_PARTICLES);

	visible.for_each<render_layer::ILLUMINATING_WANDERING_PIXELS>(cosm, [&](const auto e) {
		draw_wandering_pixels_as_sprites(wandering_pixels, e, game_images, drawing_input.make_input_for<invariants::sprite>());
	});

	renderer.call_and_clear_triangles();

	set_shader_with_matrix(shaders.circular_bars);

	const auto set_center_uniform = [&](const augs::atlas_entry& tex) {
		shaders.circular_bars->set_uniform("texture_center", tex.get_center());
	};

	augs::vertex_triangle_buffer textual_infos;

	if (viewed_character) {
		auto make_input_for = [&](const auto& tex_type, const auto meter) {
			const auto tex = necessarys.at(tex_type);

			set_center_uniform(tex);

			return draw_sentiences_hud_input {
				cone,
				visible,
				settings,
				output,
				renderer.get_special_buffer(),
				cosm,
				viewed_character,
				interp,
				global_time_seconds,
				gui_font,
				tex,
				meter
			};
		};

		std::array<assets::necessary_image_id, 3> circles = {
			assets::necessary_image_id::CIRCULAR_BAR_LARGE,
			assets::necessary_image_id::CIRCULAR_BAR_UNDER_LARGE,
			assets::necessary_image_id::CIRCULAR_BAR_UNDER_UNDER_LARGE
		};

		int current_circle = 0;

		textual_infos = draw_sentiences_hud(
			make_input_for(
				circles[current_circle++], 
				meter_id::of<health_meter_instance>()
			)
		);

		renderer.call_and_clear_triangles();

		if (settings.draw_pe_bar) {
			draw_sentiences_hud(
				make_input_for(
					circles[current_circle++], 
					meter_id::of<personal_electricity_meter_instance>()
				)
			);

			renderer.call_and_clear_triangles();
		}

		if (settings.draw_cp_bar) {
			draw_sentiences_hud(
				make_input_for(
					circles[current_circle++], 
					meter_id::of<consciousness_meter_instance>()
				)
			);

			renderer.call_and_clear_triangles();
		}
	}
	
	{
		auto draw_explosives_hud = [&](const auto tex_id, const auto type) {
			const auto tex = necessarys.at(tex_id);

			set_center_uniform(tex);

			draw_hud_for_explosives({
				output,
				renderer.get_special_buffer(),
				settings,
				interp,
				cosm,
				viewed_character,
				global_time_seconds,
				tex,
				type
			});

			renderer.call_and_clear_triangles();
		};

		draw_explosives_hud(
			assets::necessary_image_id::CIRCULAR_BAR_SMALL,
			circular_bar_type::SMALL
		);

		draw_explosives_hud(
			assets::necessary_image_id::CIRCULAR_BAR_MEDIUM,
			circular_bar_type::MEDIUM
		);

		draw_explosives_hud(
			assets::necessary_image_id::CIRCULAR_BAR_OVER_MEDIUM,
			circular_bar_type::OVER_MEDIUM
		);
	}

	set_shader_with_non_zoomed_matrix(shaders.standard);
	renderer.call_triangles(textual_infos);

	set_shader_with_matrix(shaders.exploding_rings);

	exploding_rings.draw_rings(
		output,
		renderer.specials,
		cone
	);

	renderer.call_and_clear_triangles();

	shaders.pure_color_highlight->set_as_current();

	highlights.draw_highlights(cosm, drawing_input);

	for (const auto& h : additional_highlights) {
		draw_color_highlight(cosm[h.id], h.col, drawing_input);
	}

	thunders.draw_thunders(
		line_output
	);

	renderer.call_and_clear_triangles();
	renderer.call_and_clear_lines();

	shaders.standard->set_as_current();

	flying_numbers.draw_numbers(
		gui_font,
		output, 
		cone
	);

	renderer.call_and_clear_triangles();
	renderer.call_and_clear_lines();

	shaders.standard->set_projection(matrix);

	if (in.general_atlas) {
		in.general_atlas->bind();
	}
}
