#include <cstring>
#include <thread>

#include "augs/templates/introspect.h"
#include "augs/graphics/renderer.h"
#include "augs/templates/enum_introspect.h"
#include "augs/string/format_enum.h"
#include "augs/misc/enum/enum_array.h"

#include "augs/misc/imgui/imgui_control_wrappers.h"
#include "augs/misc/imgui/imgui_enum_combo.h"
#include "augs/misc/imgui/imgui_utils.h"
#include "augs/audio/audio_context.h"

#include "augs/window_framework/platform_utils.h"
#include "augs/window_framework/window.h"

#include "view/necessary_resources.h"

#include "application/config_lua_table.h"
#include "application/gui/settings_gui.h"

void configuration_subscribers::sync_back_into(config_lua_table& into) const {
	window.sync_back_into(into.window);
}

void configuration_subscribers::apply(const config_lua_table& new_config) const {
	DEBUG_DRAWING = new_config.debug_drawing;
	
	const auto screen_size = window.get_screen_size();

	fbos.apply(screen_size, new_config.drawing);
	window.apply(new_config.window);
	audio_context.apply(new_config.audio);
	renderer.apply(new_config.renderer);
}

#define CONFIG_NVP(x) format_field_name(std::string(#x)) + "##" + std::to_string(field_id++), config.x
#define SCOPE_CFG_NVP(x) format_field_name(std::string(#x)) + "##" + std::to_string(field_id++), scope_cfg.x

void settings_gui_state::perform(
	sol::state& lua,
	const augs::path_type& config_path_for_saving,
	config_lua_table& config,
	config_lua_table& last_saved_config,
	vec2i screen_size
) {
	using namespace augs::imgui;

	centered_size_mult = 0.6f;
	
	auto settings = make_scoped_window();

	if (!settings) {
		return;
	}

	{
		auto child = scoped_child("settings view", ImVec2(0, -(ImGui::GetFrameHeightWithSpacing() + 4)));
		auto width = scoped_item_width(ImGui::GetWindowWidth() * 0.35f);

		int field_id = 0;

		{
			auto style = scoped_style_var(ImGuiStyleVar_FramePadding, []() { auto padding = ImGui::GetStyle().FramePadding; padding.x *= 4; return padding; }());

			{
				static auto labels = []() {
					static augs::enum_array<std::string, settings_pane> label_strs;
					augs::enum_array<const char*, settings_pane> c_strs;

					augs::for_each_enum_except_bounds([&c_strs](const settings_pane s) {
						label_strs[s] = format_enum(s);
						c_strs[s] = label_strs[s].c_str();
					});

					c_strs[settings_pane::GUI_STYLES] = "GUI";

					return c_strs;
				}();

				auto index = static_cast<int>(active_pane);
				ImGui::TabLabels(labels.data(), static_cast<int>(labels.size()), index, nullptr);
				active_pane = static_cast<settings_pane>(index);
			}
		}
		
		{
			auto scope = scoped_style_color(ImGuiCol_Separator, ImGui::GetStyle().Colors[ImGuiCol_Button]);
			ImGui::Separator();
		}

		auto revert = make_revert_button_lambda(config, last_saved_config);

		auto revertable_checkbox = [&](auto l, auto& f, auto&&... args) {
			checkbox(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto revertable_slider = [&](auto l, auto& f, auto&&... args) {
			slider(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto revertable_drag_rect_bounded_vec2i = [&](auto l, auto& f, auto&&... args) {
			drag_rect_bounded_vec2i(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto revertable_drag = [&](auto l, auto& f, auto&&... args) {
			drag(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto revertable_drag_vec2 = [&](auto l, auto& f, auto&&... args) {
			drag_vec2(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto revertable_color_edit = [&](auto l, auto& f, auto&&... args) {
			color_edit(l, f, std::forward<decltype(args)>(args)...);
			revert(f);
		};

		auto do_lag_simulator = [&](auto& sim) {
			//#if !IS_PRODUCTION_BUILD
			revertable_checkbox("Enable lag simulator", sim.is_enabled);

			if (sim.is_enabled) {
				auto& scope_cfg = sim.value;
				auto indent = scoped_indent();

				revertable_slider(SCOPE_CFG_NVP(latency_ms), 0.f, 200.f);
				revertable_slider(SCOPE_CFG_NVP(jitter_ms), 0.f, 100.f);
				revertable_slider(SCOPE_CFG_NVP(loss_percent), 0.f, 99.f);
				revertable_slider(SCOPE_CFG_NVP(duplicates_percent), 0.f, 99.f);
			}

			text_disabled("The network simulation is always disabled in production builds.");
			//#else
			(void)sim;
			//#endif
		};

		switch (active_pane) {
			case settings_pane::WINDOW: {
				enum_combo("Launch on game's startup", config.launch_mode);

				revertable_checkbox("Fullscreen", config.window.fullscreen);
				if (!config.window.fullscreen) {
					auto indent = scoped_indent();
					
					{
						const auto disp = augs::get_display();

						revertable_drag_rect_bounded_vec2i("Window position", config.window.position, 0.3f, vec2i{ 0, 0 }, disp.get_size());
						revertable_drag_rect_bounded_vec2i("Windowed size", config.window.size, 0.3f, vec2i{ 0, 0 }, disp.get_size());
					}

					revertable_checkbox(CONFIG_NVP(window.border));
				}

				{
					text("GUI cursor input source");

					auto indent = scoped_indent();

					int e = config.window.raw_mouse_input ? 1 : 0;
					ImGui::RadioButton("Raw (traps cursor inside the window)", &e, 1);
					
					if (ImGui::IsItemHovered()) {
						text_tooltip("Game draws its own cursor.\nWhen in GUI mode,\nforces the cursor inside the window.");
					}
#if TODO
					if (config.window.raw_mouse_input) {
						auto indent = scoped_indent();

						revertable_checkbox("But use system cursor for GUI", config.session.use_system_cursor_for_gui);
					}
#endif
					ImGui::RadioButton("System cursor", &e, 0);

					if (ImGui::IsItemHovered()) {
						text_tooltip("When in GUI mode,\nlets the cursor go outside the window.");
					}

					config.window.raw_mouse_input = e != 0;
				}

				input_text<100>(CONFIG_NVP(window.name), ImGuiInputTextFlags_EnterReturnsTrue); revert(config.window.name);
				revertable_checkbox("Automatically hide settings in-game", config.session.automatically_hide_settings_ingame);

				enum_combo("Vsync mode", config.window.vsync_mode);

				break;
			}
			case settings_pane::GRAPHICS: {
				enum_combo("Default filtering", config.renderer.default_filtering);

				{
					auto scope = scoped_indent();

					text_disabled(
						"The nearest neighbor filtering will give you a nostalgic, pixel-art feeling.\n"
						"The linear filtering is a little easier on the eyes.\n"
						"Linear filtering is enabled automatically when the camera zooms out."
					);
				}

				revertable_checkbox("Interpolate frames", config.interpolation.enabled);
				
				if (config.interpolation.enabled) {
					auto scope = scoped_indent();

					revertable_slider("Speed", config.interpolation.speed, 50.f, 1000.f);
				}

				revertable_checkbox("Highlight hovered world items", config.drawing.draw_aabb_highlighter);

				if (auto node = scoped_tree_node("Arena mode GUI")) {
					if (auto node = scoped_tree_node("Scoreboard")) {
						auto scope = scoped_indent();
						auto& scope_cfg = config.arena_mode_gui.scoreboard_settings;

						revertable_slider(SCOPE_CFG_NVP(cell_bg_alpha), 0.f, 1.f);
						
						revertable_drag_vec2(SCOPE_CFG_NVP(player_row_inner_padding), 1.f, 0, 20);

						revertable_color_edit(SCOPE_CFG_NVP(background_color));
						revertable_color_edit(SCOPE_CFG_NVP(border_color));

						revertable_slider(SCOPE_CFG_NVP(bg_lumi_mult), 0.f, 5.f);
						revertable_slider(SCOPE_CFG_NVP(text_lumi_mult), 0.f, 5.f);
						revertable_slider(SCOPE_CFG_NVP(current_player_bg_lumi_mult), 0.f, 5.f);
						revertable_slider(SCOPE_CFG_NVP(current_player_text_lumi_mult), 0.f, 5.f);
						revertable_slider(SCOPE_CFG_NVP(dead_player_bg_lumi_mult), 0.f, 1.f);
						revertable_slider(SCOPE_CFG_NVP(dead_player_bg_alpha_mult), 0.f, 1.f);
						revertable_slider(SCOPE_CFG_NVP(dead_player_text_alpha_mult), 0.f, 1.f);
						revertable_slider(SCOPE_CFG_NVP(dead_player_text_lumi_mult), 0.f, 1.f);

						revertable_slider(SCOPE_CFG_NVP(text_stroke_lumi_mult), 0.f, 1.f);
						revertable_slider(SCOPE_CFG_NVP(faction_logo_alpha), 0.f, 1.f);
						revertable_checkbox(SCOPE_CFG_NVP(dark_color_overlay_under_score));
					}

					if (auto node = scoped_tree_node("Buy menu")) {
						auto scope = scoped_indent();
						auto& scope_cfg = config.arena_mode_gui.buy_menu_settings;

						revertable_color_edit(SCOPE_CFG_NVP(disabled_bg));
						revertable_color_edit(SCOPE_CFG_NVP(disabled_active_bg));

						revertable_color_edit(SCOPE_CFG_NVP(already_owns_bg));
						revertable_color_edit(SCOPE_CFG_NVP(already_owns_active_bg));
					}

					auto& scope_cfg = config.arena_mode_gui;

					if (auto node = scoped_tree_node("Knockouts indicators")) {
						auto scope = scoped_indent();

						revertable_slider(SCOPE_CFG_NVP(between_knockout_boxes_pad), 0u, 20u);
						revertable_slider(SCOPE_CFG_NVP(inside_knockout_box_pad), 0u, 20u);
						revertable_slider(SCOPE_CFG_NVP(weapon_icon_horizontal_pad), 0u, 20u);
						revertable_slider(SCOPE_CFG_NVP(show_recent_knockouts_num), 0u, 20u);
						revertable_slider(SCOPE_CFG_NVP(keep_recent_knockouts_for_seconds), 0.f, 20.f);
						revertable_slider("Max weapon icon height (0 for no limit)", scope_cfg.max_weapon_icon_height, 0u, 100u);
					}

					if (auto node = scoped_tree_node("Money indicator")) {
						auto scope = scoped_indent();

						revertable_drag_rect_bounded_vec2i("Money indicator position", scope_cfg.money_indicator_pos, 0.3f, -vec2i(screen_size), vec2i(screen_size));
						revertable_color_edit("Money indicator color", scope_cfg.money_indicator_color);
						revertable_color_edit("Award indicator color", scope_cfg.award_indicator_color);
						revertable_slider(SCOPE_CFG_NVP(show_recent_awards_num), 0u, 20u);
						revertable_slider(SCOPE_CFG_NVP(keep_recent_awards_for_seconds), 0.f, 20.f);
					}
				}

				if (auto node = scoped_tree_node("Faction view")) {
					auto nha = [&](auto& f) {
						auto& scope_cfg = f;

						revertable_color_edit(SCOPE_CFG_NVP(normal));
						revertable_color_edit(SCOPE_CFG_NVP(hovered));
						revertable_color_edit(SCOPE_CFG_NVP(active));
					};

					auto for_faction = [&](const faction_type f) {
						auto& scope_cfg = config.faction_view.colors[f];

						revertable_color_edit(SCOPE_CFG_NVP(standard));
						revertable_color_edit(SCOPE_CFG_NVP(current_player_text));
						revertable_color_edit(SCOPE_CFG_NVP(background_dark));

						text("Background");

						{
							auto scope = scoped_indent();
							nha(scope_cfg.background);
						}
					};

					augs::for_each_enum_except_bounds([&](const faction_type f) {
						text(format_enum(f));
						auto scope = scoped_indent();
						for_faction(f);
					});
				}

				break;
			}
			case settings_pane::AUDIO: {
				revertable_slider("Music volume", config.audio_volume.music, 0.f, 1.f);
				revertable_slider("Sound effects volume", config.audio_volume.sound_effects, 0.f, 1.f);
				revertable_slider("GUI volume", config.audio_volume.gui, 0.f, 1.f);

				revertable_checkbox("Enable HRTF", config.audio.enable_hrtf);
				revertable_slider("Speed of sound (m/s)", config.audio.sound_meters_per_second, 50.f, 400.f);

				break;
			}
			case settings_pane::CONTROLS: {
				auto& scope_cfg = config.input;

				revertable_drag_vec2(SCOPE_CFG_NVP(mouse_sensitivity));
				revertable_checkbox(SCOPE_CFG_NVP(swap_mouse_buttons_in_akimbo));

				break;
			}
			case settings_pane::GAMEPLAY: {
				if (auto node = scoped_tree_node("Camera")) {
					auto& scope_cfg = config.camera;

					revertable_slider(SCOPE_CFG_NVP(look_bound_expand), 0.2f, 2.f);
					
					revertable_checkbox(SCOPE_CFG_NVP(enable_smoothing));

					if (scope_cfg.enable_smoothing) {
						auto indent = scoped_indent();

						revertable_slider(SCOPE_CFG_NVP(smoothing.averages_per_sec), 0.f, 100.f); 
						revertable_slider(SCOPE_CFG_NVP(smoothing.average_factor), 0.01f, 0.95f); 
					}

					revertable_checkbox(SCOPE_CFG_NVP(adjust_zoom_to_available_fog_of_war_size));
				}

				revertable_checkbox("Draw Personal Electricity HUD", config.drawing.draw_pe_bar);
				revertable_checkbox("Draw Consciousness HUD", config.drawing.draw_cp_bar);

				revertable_checkbox("Draw weapon laser", config.drawing.draw_weapon_laser);
				revertable_checkbox("Draw crosshairs", config.drawing.draw_crosshairs);
				revertable_checkbox("Draw area markers", config.drawing.draw_area_markers.is_enabled);

				if (config.drawing.draw_area_markers.is_enabled) {
					auto indent = scoped_indent();
					revertable_slider("Alpha", config.drawing.draw_area_markers.value, 0.f, 1.f);
				}

				if (auto node = scoped_tree_node("Game GUI")) {
					auto& scope_cfg = config.game_gui;

					revertable_checkbox(SCOPE_CFG_NVP(autodrop_magazines_of_dropped_weapons));
					revertable_checkbox(SCOPE_CFG_NVP(autodrop_holstered_armed_explosives));
					revertable_checkbox(SCOPE_CFG_NVP(autocollapse_hotbar_buttons));
				}

				if (auto node = scoped_tree_node("Fog of war")) {
					auto& scope_cfg = config.drawing.fog_of_war_appearance;
					revertable_checkbox(SCOPE_CFG_NVP(overlay_color_on_visible));
					revertable_color_edit(SCOPE_CFG_NVP(overlay_color));
				}

				// revertable_checkbox("Draw gameplay GUI", config.drawing.draw_character_gui); revert(config.drawing.draw_character_gui);
				break;
			}
			case settings_pane::CLIENT: {
				auto& scope_cfg = config.client;

				{
					auto& scope_cfg = config.arena_mode_gui;
					revertable_checkbox(SCOPE_CFG_NVP(show_client_resyncing_notifier));
				}

				if (auto node = scoped_tree_node("Lag compensation techniques")) {
					{
						auto& scope_cfg = config.simulation_receiver;
						revertable_slider(SCOPE_CFG_NVP(misprediction_smoothing_multiplier), 0.f, 3.f);
					}

					{
						auto& scope_cfg = config.lag_compensation;
						revertable_checkbox(SCOPE_CFG_NVP(confirm_controlled_character_death));

						augs::introspect(
							[&](const std::string& label, auto& field){
								revertable_checkbox(format_field_name(label), field);
							},
							scope_cfg.effect_prediction
						); 
					}
				}

				if (auto node = scoped_tree_node("Requested network settings")) {
					auto& scope_cfg = config.client.net.jitter;

					if (auto node = scoped_tree_node("Jitter")) {
						revertable_slider(SCOPE_CFG_NVP(buffer_ms), 0u, 200u);
						revertable_slider(SCOPE_CFG_NVP(merge_commands_when_above_ms), 0u, 1000u);
						revertable_slider(SCOPE_CFG_NVP(max_commands_to_squash_at_once), uint8_t(0), uint8_t(255));
					}
				}

				revertable_slider(SCOPE_CFG_NVP(max_buffered_server_commands), 0u, 10000u);
				revertable_slider(SCOPE_CFG_NVP(max_predicted_client_commands), 0u, 3000u);

				do_lag_simulator(config.client.network_simulator);

				break;
			}
			case settings_pane::SERVER: {
				auto& scope_cfg = config.server;

				revertable_slider(SCOPE_CFG_NVP(kick_if_inactive_for_secs), 0u, 300u);
				revertable_slider(SCOPE_CFG_NVP(time_limit_to_enter_game_since_connection), 0u, 300u);

				input_text<100>(SCOPE_CFG_NVP(override_default_ruleset)); revert(scope_cfg.override_default_ruleset);

				do_lag_simulator(config.server.network_simulator);

				input_text<100>(SCOPE_CFG_NVP(current_arena)); revert(scope_cfg.current_arena);

				break;
			}
			case settings_pane::EDITOR: {
				if (auto node = scoped_tree_node("General")) {
					revertable_checkbox("Enable autosave", config.editor.autosave.enabled);
	
					if (config.editor.autosave.enabled) {
						auto scope = scoped_indent();
						text("Autosave once per");
						ImGui::SameLine();
						revertable_drag("minutes", config.editor.autosave.once_every_min, 0.002, 0.05, 2000.0);

						revertable_checkbox("Autosave when window loses focus", config.editor.autosave.on_lost_focus);
					}
					
#if TODO
					text("Remember last");
					ImGui::SameLine();

					revertable_drag("commands for undoing", config.editor.remember_last_n_commands, 1, 10, 2000);
#endif

					if (auto node = scoped_tree_node("Test scene generation")) {
						auto& scope_cfg = config.editor.test_scene;
						
						revertable_slider(SCOPE_CFG_NVP(scene_tickrate), 10u, 300u);
						revertable_checkbox(SCOPE_CFG_NVP(start_bomb_mode));
					}
				}	
				
				if (auto node = scoped_tree_node("Interface")) {
					if (auto node = scoped_tree_node("Grid")) {
						auto& scope_cfg = config.editor.grid.render;

						if (auto lines = scoped_tree_node("Line colors")) {
							for (std::size_t i = 0; i < scope_cfg.line_colors.size(); ++i) {
								revertable_color_edit(std::to_string(i), scope_cfg.line_colors[i]);
							}
						}

						{
							auto& po2 = scope_cfg.maximum_power_of_two;

							revertable_slider("Maximum power of 2", po2, 3u, 20u);
							ImGui::SameLine();
							text(typesafe_sprintf("(Max grid size: %x)", 1 << po2));
						}

						revertable_slider("Alpha", scope_cfg.alpha_multiplier, 0.f, 1.f);
						revertable_slider(SCOPE_CFG_NVP(hide_grids_smaller_than), 0u, 128u);
					}

					if (auto node = scoped_tree_node("Camera")) {
						revertable_drag("Panning speed", config.editor.camera.panning_speed, 0.001f, -10.f, 10.f);
					}

					if (auto node = scoped_tree_node("\"Go to\" dialogs")) {
						revertable_slider("Width", config.editor.go_to.dialog_width, 30u, static_cast<unsigned>(screen_size.x));
						revertable_slider("Number of lines to show", config.editor.go_to.num_lines, 1u, 300u);
					}

					if (auto node = scoped_tree_node("Entity selections")) {
						auto& scope_cfg = config.editor;
						revertable_checkbox(SCOPE_CFG_NVP(keep_source_entities_selected_on_mirroring));
					}
				}

				if (auto node = scoped_tree_node("Appearance")) {
					if (auto node = scoped_tree_node("Property editor")) {
						auto& scope_cfg = config.editor.property_editor;

						revertable_color_edit(SCOPE_CFG_NVP(different_values_frame_bg));
						revertable_color_edit(SCOPE_CFG_NVP(different_values_frame_hovered_bg));
						revertable_color_edit(SCOPE_CFG_NVP(different_values_frame_active_bg));
					}

					if (auto node = scoped_tree_node("Entity highlights")) {
						{
							auto& scope_cfg = config.editor.entity_selector;

							revertable_color_edit(SCOPE_CFG_NVP(hovered_color));
							revertable_color_edit(SCOPE_CFG_NVP(hovered_dashed_line_color));

							revertable_color_edit(SCOPE_CFG_NVP(selected_color));
							revertable_color_edit(SCOPE_CFG_NVP(held_color));
						}

						{
							auto& scope_cfg = config.editor;

							revertable_color_edit(SCOPE_CFG_NVP(controlled_entity_color));
							revertable_color_edit(SCOPE_CFG_NVP(matched_entity_color));
						}
					}

					if (auto node = scoped_tree_node("Action indicator")) {
						auto& scope_cfg = config.editor.action_indicator;

						revertable_color_edit(SCOPE_CFG_NVP(bg_color));
						revertable_color_edit(SCOPE_CFG_NVP(bg_border_color));

						revertable_slider(SCOPE_CFG_NVP(max_width), 10u, 1000u);
						revertable_slider(SCOPE_CFG_NVP(show_for_ms), 0u, 20000u);

						revertable_slider(SCOPE_CFG_NVP(show_for_ms), 0u, 20000u);

						revertable_drag_vec2(SCOPE_CFG_NVP(text_padding));
						revertable_drag_vec2(SCOPE_CFG_NVP(offset));
					}

					auto& scope_cfg = config.editor;

					revertable_color_edit(SCOPE_CFG_NVP(tutorial_text_color));
					revertable_color_edit(SCOPE_CFG_NVP(rectangular_selection_color));
					revertable_color_edit(SCOPE_CFG_NVP(rectangular_selection_border_color));
				}

				if (auto node = scoped_tree_node("Player")) {
					auto& scope_cfg = config.editor.player;

					revertable_slider(SCOPE_CFG_NVP(snapshot_interval_in_steps), 400u, 5000u);
				}

				if (auto node = scoped_tree_node("Debug")) {
					auto& scope_cfg = config.editor;

					revertable_checkbox(SCOPE_CFG_NVP(save_entropies_to_live_file));
				}

				break;
			}
			case settings_pane::GUI_STYLES: {
				if (auto node = scoped_tree_node("GUI font")) {
					auto scope = scoped_indent();

					revertable_slider("Size in pixels", config.gui_fonts.gui.size_in_pixels, 5.f, 30.f);
				}

				text(
					"Note: this is the original ImGui style tweaker.\n"
					"It is used because imgui is being continuously improved,\n"
					"so keeping it up to date by ourselves would be pretty hard.\n\n"
					"To save your changes to the local configuration file,\n"
					"You need to push Save Ref and only then Save settings at the bottom."
				);
				
				ImGui::Separator();
				ImGui::ShowStyleEditor(&config.gui_style);

				break;
			}
			case settings_pane::ADVANCED: {
				text(u8"Test: いい товарищ żółćńźś");

				revertable_checkbox("Show developer console", config.session.show_developer_console);
				revertable_checkbox("Log keystrokes", config.window.log_keystrokes);
				revertable_slider("Camera query aabb mult", config.session.camera_query_aabb_mult, 0.10f, 5.f);
				
				revertable_checkbox("Draw debug lines", config.debug_drawing.enabled);

				if (config.debug_drawing.enabled) {
					auto indent = scoped_indent();

					augs::introspect(
						[&](const std::string& label, auto& field){
							if (label != "enabled") {
								revertable_checkbox(format_field_name(label), field);
							}
						},
						config.debug_drawing
					); 
				}

#if BUILD_TEST_SCENES
				text("Test scenes (%x)", "built-in");
#else
				text("Test scenes (%x)", "not built-in");
#endif

				{
					auto indent = scoped_indent();

					revertable_checkbox("Create minimal", config.test_scene.create_minimal);
					revertable_slider("Tickrate", config.test_scene.scene_tickrate, 10u, 300u);
				}

				{
					auto& scope_cfg = config.debug;
					revertable_checkbox(SCOPE_CFG_NVP(measure_atlas_uploading));
				}

				text("Content regeneration");

				{
					auto indent = scoped_indent();
					auto& scope_cfg = config.content_regeneration;

					revertable_checkbox(SCOPE_CFG_NVP(regenerate_every_time));
					ImGui::SameLine();

					text_disabled("(for benchmarking)");

					const auto concurrency = std::thread::hardware_concurrency();
					const auto t_max = concurrency * 2;

					text_disabled("(Value of 0 tells regenerators to not spawn any additional workers)");
					text_disabled(typesafe_sprintf("(std::thread::hardware_concurrency() = %x)", concurrency));

					revertable_slider(SCOPE_CFG_NVP(atlas_blitting_threads), 0u, t_max);
					revertable_slider(SCOPE_CFG_NVP(neon_regeneration_threads), 0u, t_max);
				}

				break;
			}
			default: {
				ensure(false && "Unknown settings pane type");
				break;
			}
		}
	}

	{
		auto scope = scoped_child("save revert");

		ImGui::Separator();

		if (config != last_saved_config) {
			if (ImGui::Button("Save settings")) {
				augs::timer save_timer;
				last_saved_config = config;
				config.save(lua, config_path_for_saving);
				LOG("Saved new config in: %x ms", save_timer.get<std::chrono::milliseconds>());
			}

			ImGui::SameLine();

			if (ImGui::Button("Revert settings")) {
				config = last_saved_config;
				ImGui::GetStyle() = config.gui_style;
			}
		}
	}
}

#undef CONFIG_NVP
