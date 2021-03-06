#pragma once
#include "view/mode_gui/arena/arena_gui_mixin.h"
#include "application/setups/draw_setup_gui_input.h"

template <class D>
std::optional<camera_eye> arena_gui_mixin<D>::find_current_camera_eye() const {
	const auto& self = static_cast<const D&>(*this);

	if (!self.is_gameplay_on()) {
		return camera_eye();
	}

	return self.get_arena_handle().on_mode(
		[&](const auto& typed_mode) -> std::optional<camera_eye> {
			if (const auto player = typed_mode.find(self.get_local_player_id())) {
				if (player->faction == faction_type::SPECTATOR) {
					return camera_eye();
				}
			}

			return std::nullopt;
		}
	);
}

template <class D>
setup_escape_result arena_gui_mixin<D>::escape() {
	auto& self = static_cast<D&>(*this);

	if (!self.is_gameplay_on()) {
		return setup_escape_result::IGNORE;
	}

	if (arena_gui.escape()) {
		return setup_escape_result::JUST_FETCH;
	}

	return setup_escape_result::IGNORE;
}

template <class D>
custom_imgui_result arena_gui_mixin<D>::perform_custom_imgui(
	const perform_custom_imgui_input in
) {
	auto& self = static_cast<D&>(*this);

	if (!self.is_gameplay_on()) {
		return custom_imgui_result::NONE;
	}

	const auto game_screen_top = 0.f;

	const auto draw_mode_in = draw_mode_gui_input { 
		game_screen_top, 
		self.get_local_player_id(), 
		in.game_atlas,
		in.config
	};

	self.get_arena_handle().on_mode_with_input(
		[&](const auto& typed_mode, const auto& mode_input) {
			const auto new_entropy = arena_gui.perform_imgui(
				draw_mode_in, 
				typed_mode, 
				mode_input
			);

			self.control(new_entropy);
		}
	);

	return custom_imgui_result::NONE;
}

template <class D>
bool arena_gui_mixin<D>::handle_input_before_imgui(
	const handle_input_before_imgui_input in
) {
	const auto& self = static_cast<const D&>(*this);

	if (!self.is_gameplay_on()) {
		return false;
	}

	(void)in;
	return false;
}

template <class D>
bool arena_gui_mixin<D>::handle_input_before_game(
	const handle_input_before_game_input in
) {
	const auto& self = static_cast<const D&>(*this);

	if (!self.is_gameplay_on()) {
		return false;
	}

	if (arena_gui.control({ in.app_controls, in.common_input_state, in.e })) { 
		return true;
	}

	return false;
}

template <class D>
void arena_gui_mixin<D>::draw_custom_gui(const draw_setup_gui_input& in) const {
	const auto& self = static_cast<const D&>(*this);

	if (!self.is_gameplay_on()) {
		return;
	}

	const auto game_screen_top = 0.f;

	const auto draw_mode_in = draw_mode_gui_input { 
		game_screen_top,
		self.get_local_player_id(), 
		in.images_in_atlas,
		in.config
	};

	self.get_arena_handle().on_mode_with_input(
		[&](const auto& typed_mode, const auto& mode_input) {
			arena_gui.draw_mode_gui(in, draw_mode_in, typed_mode, mode_input);
		}
	);
}

template <class D>
bool arena_gui_mixin<D>::requires_cursor() const {
	return arena_gui.requires_cursor();
}
