#pragma once
#include <string>
#include "augs/math/vec2.h"

class game_window;

class config_values {
public:
	int launch_mode = 0;

	float recording_replay_speed = 1.f;

	unsigned determinism_test_cloned_cosmoi_count = 0;

	bool debug_disable_cursor_clipping = false;

	std::string connect_address;
	unsigned short connect_port = 0;
	unsigned short server_port = 0;
	unsigned short alternative_port = 0;

	std::string nickname;
	std::string debug_second_nickname;

	vec2 mouse_sensitivity;

	unsigned tickrate = 0;

	unsigned jitter_buffer_ms = 0;
	unsigned client_commands_jitter_buffer_ms = 0;

	float interpolation_speed = 0;

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

	void get_values(game_window&);
};