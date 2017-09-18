#include "game/transcendental/cosmos.h"
#include "game/components/sentience_component.h"

#include "augs/drawing/drawing.h"
#include "augs/gui/text/printer.h"
#include "augs/templates/visit_gettable.h"

#include "view/game_gui/elements/action_button.h"
#include "view/game_gui/game_gui_context.h"
#include "view/game_gui/game_gui_system.h"

#include "view/viewables/all_viewables_declarations.h"
#include "view/viewables/game_image.h"

using namespace augs::gui::text;

void action_button::draw(
	const viewing_game_gui_context context,
	const const_this_in_item this_id
) {
	const auto intent_for_this_button = static_cast<game_gui_intent_type>(
		static_cast<int>(game_gui_intent_type::SPECIAL_ACTION_BUTTON_1) + this_id.get_location().index
	);

	if (
		const auto bound_key = key_or_default(
			context.get_input_information(),
			intent_for_this_button
		);
		bound_key != augs::event::keys::key::INVALID
	) {
		const auto& sentience = context.get_subject_entity().get<components::sentience>();
		const auto bound_spell = get_bound_spell(context, this_id);

		const auto& cosmos = context.get_cosmos();

		const auto output = context.get_output();
		const auto now = cosmos.get_timestamp();
		const auto dt = cosmos.get_fixed_delta();

		const auto& necessarys = context.get_necessary_images();
		const auto& game_images = context.get_game_images();
		const auto& gui_font = context.get_gui_font();

		const auto absolute_rect = context.get_tree_entry(this_id).get_absolute_rect();
		if (bound_spell.is_set()) {
			visit_gettable(
				sentience.spells,
				bound_spell,
				[&](const auto& spell){
					const auto spell_data = get_meta_of(spell, cosmos.get_common_state().spells);

					const auto& pe = sentience.get<personal_electricity_meter_instance>();
					const bool has_enough_mana = pe.value >= spell_data.common.personal_electricity_required;
					const float required_mana_ratio = std::min(1.f, pe.value / static_cast<float>(spell_data.common.personal_electricity_required));

					rgba inside_col = white;

					inside_col.a = 220;

					const auto& detector = this_id->detector;
					const bool is_pushed = detector.current_appearance == augs::gui::appearance_detector::appearance::pushed;

					if (detector.is_hovered) {
						inside_col.a = 255;
					}

					assets::game_image_id inside_tex = assets::game_image_id::INVALID;
					const assets::necessary_image_id border_tex = assets::necessary_image_id::SPELL_BORDER;

					rgba border_col;

					inside_tex = spell_data.appearance.icon;
					border_col = spell_data.common.associated_color;

					if (!has_enough_mana) {
						border_col = border_col.get_desaturated();
					}

					if (inside_tex != assets::game_image_id::INVALID) {
						ensure(border_tex != assets::necessary_image_id::INVALID);

						const auto absolute_icon_rect = ltrb(vec2i(0, 0), game_images.at(inside_tex).get_size()).place_in_center_of(absolute_rect);
						const bool draw_partial_colorful_rect = false;

						if (has_enough_mana) {
							output.aabb(
								game_images.at(inside_tex).texture_maps[texture_map_type::DIFFUSE],
								absolute_icon_rect,
								inside_col
							);
						}
						else {
							output.aabb(
								game_images.at(inside_tex).texture_maps[texture_map_type::DESATURATED],
								absolute_icon_rect,
								inside_col
							);

							if (draw_partial_colorful_rect) {
								auto colorful_rect = absolute_icon_rect;
								const auto colorful_height = static_cast<int>(absolute_icon_rect.h() * required_mana_ratio);
								colorful_rect.t = absolute_icon_rect.b - colorful_height;
								colorful_rect.b = colorful_rect.t + colorful_height;

								output.aabb_clipped(
									game_images.at(inside_tex).texture_maps[texture_map_type::DIFFUSE],
									ltrb(absolute_icon_rect),
									ltrb(colorful_rect),
									inside_col
								);
							}
						}

						bool is_still_cooled_down = false;

						{
							const auto all_cooldown = sentience.cast_cooldown_for_all_spells;
							const auto this_cooldown = spell.cast_cooldown;

							const auto effective_cooldown_ratio =
								all_cooldown.get_remaining_time_ms(now, dt) > this_cooldown.get_remaining_time_ms(now, dt)
								?
								all_cooldown.get_ratio_of_remaining_time(now, dt) : this_cooldown.get_ratio_of_remaining_time(now, dt);

							if (effective_cooldown_ratio > 0.f) {
								output.rectangular_clock(ltrb(absolute_icon_rect), rgba { 0, 0, 0, 200 }, effective_cooldown_ratio);
								is_still_cooled_down = true;
							}
						}
						
						if (is_still_cooled_down) {
							border_col.a -= 150;
						}

						if (this_id->detector.is_hovered) {
							border_col = rgba(220, 220, 220, 255);
						}

						if (is_pushed) {
							border_col = white;
						}

						output.gui_box_center_tex(
							necessarys.at(border_tex),
							context,
							this_id, 
							border_col
						);

						auto label_col = has_enough_mana ? border_col : white;
						label_col.a = 255;

						const auto label_text = formatted_string { key_to_wstring(bound_key), { gui_font, label_col } };
						const auto label_bbox = get_text_bbox(label_text);

						print_stroked(
							output,
							absolute_rect.right_bottom() - label_bbox - vec2(4, 0),
							label_text
						);
					}
				}
			);
		}
		else {
			rgba inside_col, border_col;

			inside_col = cyan;

			border_col = inside_col;
			inside_col.a = 4 * 5;
			border_col.a = 220;

			if (this_id->detector.is_hovered || this_id->detector.current_appearance == augs::gui::appearance_detector::appearance::pushed) {
				inside_col.a = 12 * 5;
				border_col.a = 255;
			}

			const auto inside_tex = assets::necessary_image_id::ACTION_BUTTON_FILLED;
			const auto border_tex = assets::necessary_image_id::ACTION_BUTTON_BORDER;

			output.gui_box_center_tex(necessarys.at(inside_tex), context, this_id, inside_col);
			output.gui_box_center_tex(necessarys.at(border_tex), context, this_id, border_col);
			
			const auto label_text = formatted_string{
				key_to_wstring(bound_key),
				{ context.get_gui_font(), border_col }
			};

			const auto label_bbox = get_text_bbox(label_text);

			print_stroked(
				output,
				absolute_rect.get_center() - label_bbox / 2 + vec2(0, 2),
				label_text
			);
		}
	}
}

spell_id action_button::get_bound_spell(
	const const_game_gui_context context,
	const const_this_in_item this_id
) {
	const auto bound_spell = this_id->bound_spell;
	const auto& sentience = context.get_subject_entity().get<components::sentience>();

	if (bound_spell.is_set() && sentience.is_learned(bound_spell)) {
		return bound_spell;
	}

	return spell_id();
}

void action_button::advance_elements(
	const game_gui_context context,
	const this_in_item this_id,
	const augs::delta dt
) {
	base::advance_elements(context, this_id, dt);

	if (this_id->detector.is_hovered) {
		this_id->elapsed_hover_time_ms += dt.in_milliseconds();
	}

	if (this_id->detector.current_appearance == augs::gui::appearance_detector::appearance::pushed) {
		this_id->elapsed_hover_time_ms = this_id->hover_highlight_duration_ms;
	}
}

void action_button::respond_to_events(
	const game_gui_context context,
	const this_in_item this_id,
	const gui_entropy& entropies
) {
	base::respond_to_events(context, this_id, entropies);

	for (const auto& info : entropies.get_events_for(this_id)) {
		this_id->detector.update_appearance(info);

		if (info.msg == gui_event::lclick) {
			const auto bound_spell = get_bound_spell(context, this_id);
			
			if (bound_spell.is_set()) {
				context.get_game_gui_system().spell_requests[context.get_subject_entity()] = bound_spell;
			}
		}

		if (info.msg == gui_event::hover) {
			this_id->elapsed_hover_time_ms = 0.f;
		}

		if (info.msg == gui_event::lfinisheddrag) {
		}
	}
}

void action_button::rebuild_layouts(
	const game_gui_context context,
	const this_in_item this_id
) {
	base::rebuild_layouts(context, this_id);
}