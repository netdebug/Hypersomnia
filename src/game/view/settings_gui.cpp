#include "augs/misc/imgui_utils.h"
#include "viewing_session.h"
#include "augs/window_framework/platform_utils.h"
#include "augs/window_framework/window.h"
#include "generated/introspectors.h"

static void ShowHelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(450.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static bool operator==(const ImVec2 a, const ImVec2 b) {
	return a.x == b.x && a.y == b.y;
};

#define CONFIG_NVP(x) format_field_name(std::string(#x)) + "##" + std::to_string(field_id++), config.x

void viewing_session::perform_settings_gui(
	augs::window::glwindow& window
) {
	const auto config_before_change = config;
	auto& state = config_gui;

	if (show_settings) {
		ImGui::Begin("Settings", &show_settings);

		ImGui::BeginChild("settings view", ImVec2(0, -(ImGui::GetItemsLineHeightWithSpacing() + 4)));
		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.35f);

		int field_id = 0;
		using namespace augs::imgui;
		const auto disp = augs::window::get_display();

		thread_local std::vector<const char*> tabs = { "Window", "Graphics", "Audio", "Controls", "GUI styles" };

		auto tab_padding = ImGui::GetStyle().FramePadding;
		tab_padding.x *= 4;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, tab_padding);
		ImGui::TabLabels(tabs.data(), tabs.size(), state.active_pane, nullptr);
		ImGui::PopStyleVar();

		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Button]);
		ImGui::Separator();
		ImGui::PopStyleColor();

		auto revert = make_revert_button_lambda(config, last_saved_config);

		int pane = 0;

		if (state.active_pane == pane++) {
			checkbox(CONFIG_NVP(fullscreen)); revert(config.fullscreen);
			if (!config.fullscreen) {
				ImGui::Indent();

				{
					vec2i lower;
					vec2i upper = disp.get_size();
					ImGui::DragIntN("Window position", &config.window_position.x, 2, 0.3f, &lower.x, &upper.x, "%.0f");
					revert(config.window_position);
				}

				{
					vec2i lower;
					vec2i upper = disp.get_size();
					ImGui::DragIntN("Windowed size", &config.windowed_size.x, 2, 0.3f, &lower.x, &upper.x, "%.0f");
					revert(config.windowed_size);
				}

				checkbox(CONFIG_NVP(window_border)); revert(config.window_border);
				checkbox(CONFIG_NVP(enable_cursor_clipping)); revert(config.enable_cursor_clipping);
				ImGui::Unindent();
			}

			input_text<100>(CONFIG_NVP(window_name));
			revert(config.window_name);
		}
		else if (state.active_pane == pane++) {

		}
		else if (state.active_pane == pane++) {
			slider(CONFIG_NVP(music_volume), 0.f, 1.f); revert(config.music_volume);
			slider(CONFIG_NVP(sound_effects_volume), 0.f, 1.f); revert(config.sound_effects_volume);
			slider("GUI volume", config.gui_volume, 0.f, 1.f); revert(config.gui_volume);
			checkbox("Enable HRTF", config.enable_hrtf); revert(config.enable_hrtf);
		}
		else if (state.active_pane == pane++) {

		}
		else if (state.active_pane == pane++) {
			ImGuiStyle& style = config.gui_style;
			const ImGuiStyle& last_saved_style = last_saved_config.gui_style;

			if (ImGui::TreeNode("Rendering"))
			{
				ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines); revert(style.AntiAliasedLines);
				ImGui::Checkbox("Anti-aliased shapes", &style.AntiAliasedShapes); revert(style.AntiAliasedShapes);
				ImGui::PushItemWidth(100);

				ImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.02f, 0.10f, FLT_MAX, NULL, 2.0f);
				if (style.CurveTessellationTol < 0.0f) style.CurveTessellationTol = 0.10f;
				revert(style.CurveTessellationTol);

				ImGui::SliderFloat("Global Alpha", &style.Alpha, 0.20f, 1.0f, "%.2f"); revert(style.Alpha);// Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f"); revert(style.WindowPadding);
				ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 16.0f, "%.0f"); revert(style.WindowRounding);
				ImGui::SliderFloat("ChildWindowRounding", &style.ChildWindowRounding, 0.0f, 16.0f, "%.0f"); revert(style.ChildWindowRounding);
				ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f"); revert(style.FramePadding);
				ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 16.0f, "%.0f"); revert(style.FrameRounding);
				ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f"); revert(style.ItemSpacing);
				ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f"); revert(style.ItemInnerSpacing);
				ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding, 0.0f, 10.0f, "%.0f"); revert(style.TouchExtraPadding);
				ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f"); revert(style.IndentSpacing);
				ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f"); revert(style.ScrollbarSize);
				ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 16.0f, "%.0f"); revert(style.ScrollbarRounding);
				ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f"); revert(style.GrabMinSize);
				ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 16.0f, "%.0f"); revert(style.GrabRounding);
				ImGui::Text("Alignment");
				ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign, 0.0f, 1.0f, "%.2f"); revert(style.WindowTitleAlign);
				ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f"); ImGui::SameLine(); ShowHelpMarker("Alignment applies when a button is larger than its text content."); revert(style.ButtonTextAlign);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Colors"))
			{
				static int output_dest = 0;
				static bool output_only_modified = false;

				static ImGuiColorEditMode edit_mode = ImGuiColorEditMode_RGB;
				ImGui::RadioButton("RGB", &edit_mode, ImGuiColorEditMode_RGB);
				ImGui::SameLine();
				ImGui::RadioButton("HSV", &edit_mode, ImGuiColorEditMode_HSV);
				ImGui::SameLine();
				ImGui::RadioButton("HEX", &edit_mode, ImGuiColorEditMode_HEX);
				//ImGui::Text("Tip: Click on colored square to change edit mode.");

				static ImGuiTextFilter filter;
				filter.Draw("Filter colors", 200);

				ImGui::BeginChild("#colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
				ImGui::PushItemWidth(-250);
				ImGui::ColorEditMode(edit_mode);
				for (int i = 0; i < ImGuiCol_COUNT; i++)
				{
					const char* name = ImGui::GetStyleColName(i);
					if (!filter.PassFilter(name))
						continue;
					ImGui::PushID(i);
					ImGui::ColorEdit4(name, (float*)&style.Colors[i], true);
					if (memcmp(&style.Colors[i], &last_saved_style.Colors[i], sizeof(ImVec4)) != 0)
					{
						ImGui::SameLine(); if (ImGui::Button("Revert")) style.Colors[i] = last_saved_style.Colors[i];
					}
					ImGui::PopID();
				}
				ImGui::EndChild();

				ImGui::TreePop();
			}

			ImGui::GetStyle() = style;
		}

		ImGui::PopItemWidth();
		ImGui::EndChild();

		{
			const bool has_config_changed =
				!augs::introspective_compare(
					config,
					last_saved_config
				)
			;

			ImGui::BeginChild("save revert");

			ImGui::Separator();

			if (has_config_changed) {
				if (ImGui::Button("Save settings")) {
					augs::timer save_timer;
					last_saved_config = config;
					config.save("config.local.lua");
					LOG("Saved new config in: %x ms", save_timer.get<std::chrono::milliseconds>());
				}

				ImGui::SameLine();

				if (ImGui::Button("Revert settings")) {
					config = last_saved_config;
					ImGui::GetStyle() = config.gui_style;
				}
			}

			ImGui::EndChild();
		}

		ImGui::End();
	}

	/* Update non-immediate values */

	apply_changes(config, config_before_change, window);
	apply_changes(config, config_before_change, *this);
	apply_changes(config, config_before_change, augs::renderer::get_current());
}

#undef CONFIG_NVP