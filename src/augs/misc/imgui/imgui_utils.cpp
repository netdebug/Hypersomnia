#include "imgui_utils.h"

#include <imgui/imgui_internal.h>

#include "augs/filesystem/file.h"

#include "augs/image/image.h"
#include "augs/image/font.h"
#include "augs/graphics/texture.h"
#include "augs/window_framework/window.h"

#include "augs/window_framework/platform_utils.h"

using namespace ImGui;

#if PLATFORM_UNIX
static const char* augs_GetClipboardText(void*) {
	thread_local std::string sss;
	sss = augs::get_clipboard_data();
	return sss.c_str();
}

static void augs_SetClipboardText(void*, const char* text) {
	augs::set_clipboard_data(text);
}
#endif

namespace augs {
	namespace imgui {
		void init(
			const char* const ini_filename,
			const char* const log_filename,
			const ImGuiStyle& initial_style
		) {
			ImGui::CreateContext();

			auto& io = GetIO();
			
			using namespace augs::event::keys;

			auto map_key = [&io](auto im, auto aug) {
				io.KeyMap[im] = int(aug);
			};

			map_key(ImGuiKey_Tab, key::TAB);
			map_key(ImGuiKey_LeftArrow, key::LEFT);
			map_key(ImGuiKey_RightArrow, key::RIGHT);
			map_key(ImGuiKey_UpArrow, key::UP);
			map_key(ImGuiKey_DownArrow, key::DOWN);
			map_key(ImGuiKey_PageUp, key::PAGEUP);
			map_key(ImGuiKey_PageDown, key::PAGEDOWN);
			map_key(ImGuiKey_Home, key::HOME);
			map_key(ImGuiKey_End, key::END);
			map_key(ImGuiKey_Delete, key::DEL);
			map_key(ImGuiKey_Backspace, key::BACKSPACE);
			map_key(ImGuiKey_Enter, key::ENTER);
			map_key(ImGuiKey_Escape, key::ESC);
			map_key(ImGuiKey_A, key::A);
			map_key(ImGuiKey_C, key::C);
			map_key(ImGuiKey_V, key::V);
			map_key(ImGuiKey_X, key::X);
			map_key(ImGuiKey_Y, key::Y);
			map_key(ImGuiKey_Z, key::Z);

			io.IniFilename = ini_filename;
			io.LogFilename = log_filename;

#if PLATFORM_UNIX
			io.SetClipboardTextFn = augs_SetClipboardText;
			io.GetClipboardTextFn = augs_GetClipboardText;
#endif

			io.MouseDoubleClickMaxDist = 100.f;
			GetStyle() = initial_style;
		}

		void setup_input(
			local_entropy& window_inputs,
			const decltype(ImGuiIO::DeltaTime) delta_seconds,
			const vec2i screen_size
		) {
			using namespace event;
			using namespace event::keys;

			auto& io = GetIO();

			io.MouseDrawCursor = false;

			for (const auto& in : window_inputs) {
				if (in.msg == message::mousemotion) {
					io.MousePos = ImVec2(in.data.mouse.pos);
				}
				else if (in.was_pressed(key::LMOUSE)) {
					io.MouseDown[0] = true;
				}
				else if (in.was_released(key::LMOUSE)) {
					io.MouseDown[0] = false;
				}
				else if (in.was_pressed(key::RMOUSE)) {
					io.MouseDown[1] = true;
				}
				else if (in.was_released(key::RMOUSE)) {
					io.MouseDown[1] = false;
				}
				else if (in.msg == message::wheel) {
					io.MouseWheel = static_cast<float>(in.data.scroll.amount);
				}
				else if (in.msg == message::keydown) {
					io.KeysDown[static_cast<int>(in.data.key.key)] = true;
				}
				else if (in.msg == message::keyup) {
					io.KeysDown[static_cast<int>(in.data.key.key)] = false;
				}
				else if (in.msg == message::character) {
					const auto c = in.data.character.code_point;

					// TODO:
					// FIXME: Losing characters that don't fit in 2 bytes (imgui issue)
					if (c < 0x10000) {
						io.AddInputCharacter(c);
					}
				}

				io.KeyCtrl = io.KeysDown[static_cast<int>(keys::key::LCTRL)] || io.KeysDown[static_cast<int>(keys::key::RCTRL)];
				io.KeyShift = io.KeysDown[static_cast<int>(keys::key::LSHIFT)] || io.KeysDown[static_cast<int>(keys::key::RSHIFT)];
				io.KeyAlt = io.KeysDown[static_cast<int>(keys::key::LALT)] || io.KeysDown[static_cast<int>(keys::key::RALT)];
			}

			io.DeltaTime = delta_seconds;
			io.DisplaySize = ImVec2(screen_size);
		}

#if 0
		void setup_input(
			event::state& state,
			const decltype(ImGuiIO::DeltaTime) delta_seconds,
			const vec2i screen_size
		) {
			using namespace event;
			using namespace event::keys;

			auto& io = GetIO();

			io.MouseDrawCursor = false;
			io.MousePos = vec2(state.mouse.pos);
			io.MouseDown[0] = state.keys[key::LMOUSE];
			io.MouseDown[1] = state.keys[key::RMOUSE];
				else if (
					in.msg == message::ldown
					|| in.msg == message::ldoubleclick
					|| in.msg == message::ltripleclick
					) {
					io.MouseDown[0] = true;
				}
				else if (in.msg == message::lup) {
					io.MouseDown[0] = false;
				}
				else if (
					in.msg == message::rdown
					|| in.msg == message::rdoubleclick
					) {
					io.MouseDown[1] = true;
				}
				else if (in.msg == message::rup) {
					io.MouseDown[1] = false;
				}
				else if (in.msg == message::wheel) {
					io.MouseWheel = static_cast<float>(in.data.scroll.amount);
				}
				else if (in.msg == message::keydown) {
					io.KeysDown[static_cast<int>(in.data.key.key)] = true;
				}
				else if (in.msg == message::keyup) {
					io.KeysDown[static_cast<int>(in.data.key.key)] = false;
				}
				else if (in.msg == message::character) {
					io.AddInputCharacter(in.data.character.code_point);
				}
			}

			io.KeyCtrl = io.KeysDown[static_cast<int>(keys::key::LCTRL)] || io.KeysDown[static_cast<int>(keys::key::RCTRL)];
			io.KeyShift = io.KeysDown[static_cast<int>(keys::key::LSHIFT)] || io.KeysDown[static_cast<int>(keys::key::RSHIFT)];
			io.KeyAlt = io.KeysDown[static_cast<int>(keys::key::LALT)] || io.KeysDown[static_cast<int>(keys::key::RALT)];

			io.DeltaTime = delta_seconds;
			io.DisplaySize = vec2(screen_size);
		}
#endif
		void render() {
			Render();
		}

		void neutralize_mouse() {
			auto& io = GetIO();

			io.MousePos = { -1, -1 };

			for (auto& m : io.MouseDown) {
				m = false;
			}
		}
		
		image create_atlas_image(const font_loading_input& in) {
			auto& io = GetIO();

			unsigned char* pixels = nullptr;
			int width = 0;
			int height = 0;

			std::vector<ImWchar> ranges;

			{
				auto unicode_ranges = in.unicode_ranges;

				if (in.add_japanese_ranges) {
					concat_ranges(unicode_ranges, ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
				}

				for (const auto& r : unicode_ranges) {
					ranges.push_back(r.first);
					ranges.push_back(r.second);
				}

				ranges.push_back(0);
			}

#if TODO_MANY_FONTS
			io.Fonts->AddFontDefault();

			ImFontConfig config;
			config.MergeMode = true;
#endif

			if (!augs::exists(in.source_font_path)) {
				throw imgui_init_error("Failed to load %x for reading.", augs::filename_first(in.source_font_path));
			}

			const auto str_path = in.source_font_path.string();

			io.Fonts->AddFontFromFileTTF(str_path.c_str(), in.size_in_pixels, nullptr, ranges.data());
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

			LOG("IMGUI FONT ATLAS SIZE: %xx%x", width, height);

			io.Fonts->TexID = reinterpret_cast<void*>(0);

			return { pixels, vec2i{ width, height } };
		}

		graphics::texture create_atlas(const font_loading_input& in) {
			return create_atlas_image(in);
		}

		bool is_hovered_with_hand_cursor() {
			// TODO: when all else is done, we may want to do this.
			return false;// IsAnyItemHovered() && GetCurrentContext()->HoveredIdHandCursor;
		}

		void center_next_window(const float size_multiplier, const ImGuiCond cond) {
			const auto screen_size = vec2(GetIO().DisplaySize);
			const auto initial_settings_size = screen_size * size_multiplier;

			set_next_window_rect(
				{
					{ screen_size / 2 - initial_settings_size / 2 },
					initial_settings_size,
				},
				cond
			);
		}

		void set_next_window_rect(const xywh r, const ImGuiCond cond) {
			SetNextWindowPos(ImVec2(r.get_position()), cond);
			SetNextWindowSize(ImVec2(r.get_size()), cond);
		}
	}
}
