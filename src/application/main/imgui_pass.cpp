#include "augs/misc/imgui/imgui_utils.h"
#include "augs/misc/timing/delta.h"

#include "application/config_lua_table.h"
#include "application/gui/settings_gui.h"
#include "application/main/imgui_pass.h"

#include "augs/misc/imgui/imgui_scope_wrappers.h"
#include "augs/misc/imgui/imgui_control_wrappers.h"

void perform_imgui_pass(
	augs::local_entropy& window_inputs,
	const vec2i screen_size,
	const augs::delta delta,
	config_lua_table& config,
	config_lua_table& last_saved_config,
	const augs::path_type& path_for_saving_config,
	settings_gui_state& settings_gui,
	sol::state& lua,
	std::function<void()> custom_imgui_logic,

	const bool ingame_menu_active,
	const bool has_gameplay_setup,

	const bool should_freeze_cursor,

	const bool float_tests_succeeded
) {
	augs::imgui::setup_input(
		window_inputs,
		delta.in_seconds(),
		screen_size
	);

	if (should_freeze_cursor) {
		augs::imgui::neutralize_mouse();
	}

	ImGui::NewFrame();

	/* 
		Don't show settings if we're in-game
		and a setting was specified to hide it automatically when playing
	*/

	if (
		const bool should_hide_settings =
			config.session.automatically_hide_settings_ingame
			&& has_gameplay_setup
			&& !ingame_menu_active
		;

		!should_hide_settings
	) {
		settings_gui.perform(
			lua,
			path_for_saving_config,
			config,
			last_saved_config,
			screen_size
		);
	}

	{
		thread_local bool show = true;
		thread_local bool dont_show = false;

		if (!float_tests_succeeded && show) {
			augs::imgui::center_next_window(0.2f);

			auto warning = augs::imgui::scoped_window("Warning", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

			augs::imgui::text(
				"Floating-point consistency tests have failed.\n"
				"You might experience frequent disconnects from the official servers.\n"
			);

			augs::imgui::text_color("Inform the developers immediately.", orange);

			augs::imgui::checkbox("Don't show this message again", dont_show);

			ImGui::Separator();

			if (ImGui::Button("    OK    ")) {
				show = false;

				if (dont_show) {
					config.perform_float_consistency_test = false;
					last_saved_config.perform_float_consistency_test = false;

					last_saved_config.save(lua, path_for_saving_config);
				}
			}
		}
	}

	if (!ingame_menu_active) {
		custom_imgui_logic();
	}

	augs::imgui::render();

	window_inputs = augs::imgui::filter_inputs(window_inputs);
}