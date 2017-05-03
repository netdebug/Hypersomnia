#pragma once
#include <string>
#include "augs/math/vec2.h"
#include "game/enums/input_recording_type.h"

#include "augs/padding_byte.h"
#include "augs/graphics/pixel.h"
#include "augs/templates/introspect.h"

#include "game/transcendental/entity_handle_declaration.h"
#include "augs/templates/introspect.h"

#include "application/config_structs/hotbar_settings.h"
#include "application/config_structs/debug_drawing_settings.h"

class game_window;

namespace sol {
	class state;
}

class config_lua_table {
public:
	// GEN INTROSPECTOR class config_lua_table
	int launch_mode = 0;
	int input_recording_mode = 0;

	float recording_replay_speed = 1.f;

	unsigned determinism_test_cloned_cosmoi_count = 0;

	std::string window_name = "example";
	bool fullscreen = false;
	bool window_border = 1;
	unsigned window_x = 100;
	unsigned window_y = 10;
	unsigned bpp = 24;
	unsigned resolution_w = 1280;
	unsigned resolution_h = 768;
	bool doublebuffer = true;

	bool check_content_integrity_every_launch = false;
	bool save_regenerated_atlases_as_binary = false;
	bool debug_regenerate_content_every_launch = false;
	unsigned packer_detail_max_atlas_size = 8192;

	bool debug_run_unit_tests = false;
	bool debug_log_successful_unit_tests = false;
	bool debug_break_on_unit_test_failure = false;

	bool enable_hrtf = true;
	unsigned max_number_of_sound_sources = 0u;

	std::string audio_output_device;

	float sound_effects_volume = 1.f;
	float music_volume = 1.f;

	bool debug_disable_cursor_clipping = false;

	std::string connect_address;
	unsigned short connect_port = 0;
	unsigned short server_port = 0;
	unsigned short alternative_port = 0;

	std::string nickname;
	std::string debug_second_nickname;

	vec2 mouse_sensitivity;

	unsigned default_tickrate = 0;

	unsigned jitter_buffer_ms = 0;
	unsigned client_commands_jitter_buffer_ms = 0;

	float interpolation_speed = 0;
	float misprediction_smoothing_multiplier = 0.5;

	int debug_var = 0;
	bool debug_randomize_entropies_in_client_setup = 0;
	unsigned debug_randomize_entropies_in_client_setup_once_every_steps = 0;

	bool server_launch_http_daemon = 0;
	unsigned short server_http_daemon_port = 0;
	std::string server_http_daemon_html_file_path;

	std::string db_path;
	std::string survey_num_file_path;
	std::string post_data_file_path;
	std::string last_session_update_link;

	std::string director_input_scene_entropy_path;
	std::string choreographic_input_scenario_path;
	std::string menu_intro_scene_entropy_path;

	std::string menu_theme_path;

	double rewind_intro_scene_by_secs = 3.5;
	double start_menu_music_at_secs = 0.f;

	bool skip_credits = false;
	std::string latest_news_url;
	
	debug_drawing_settings debug;
	hotbar_settings hotbar;
	// END GEN INTROSPECTOR

	void get_values(sol::state&);

	enum class launch_type {
		MAIN_MENU,

		LOCAL,
		LOCAL_DETERMINISM_TEST,

		DIRECTOR,
		CHOREOGRAPHIC,

		ONLY_CLIENT,
		ONLY_SERVER,

		CLIENT_AND_SERVER,
		TWO_CLIENTS_AND_SERVER,

		COUNT
	};
	
	launch_type get_launch_mode() const;
	input_recording_type get_input_recording_mode() const;
};