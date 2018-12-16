#pragma once
#include "augs/math/camera_cone.h"
#include "game/detail/render_layer_filter.h"
#include "application/setups/server/server_start_input.h"
#include "application/intercosm.h"
#include "game/cosmos/cosmos.h"
#include "game/cosmos/entity_handle.h"

#include "application/setups/default_setup_settings.h"
#include "application/input/entropy_accumulator.h"

#include "application/setups/setup_common.h"
#include "game/modes/all_mode_includes.h"
#include "game/modes/mode_entropy.h"
#include "game/modes/ruleset_id.h"

#include "augs/network/network_types.h"
#include "application/setups/server/server_vars.h"
#include "application/setups/server/server_client_state.h"
#include "application/predefined_rulesets.h"
#include "application/arena/mode_and_rules.h"
#include "augs/readwrite/memory_stream_declaration.h"
#include "augs/misc/serialization_buffers.h"

#include "application/network/server_step_entropy.h"
#include "view/mode_gui/arena/arena_gui_mixin.h"
#include "application/network/network_common.h"

struct config_lua_table;
struct draw_setup_gui_input;

namespace net_messages {
	struct client_welcome;
}

struct add_to_arena_input {
	client_id_type client_id;
	entity_name_str nickname;
};

class server_adapter;

class server_setup : 
	public default_setup_settings,
	public arena_gui_mixin<server_setup> /* For the admin player */
{
	/* This is loaded from the arena folder */
	intercosm scene;
	cosmos_solvable_significant initial_signi;

	predefined_rulesets rulesets;

	/* Other replicated state */
	online_mode_and_rules current_mode;
	server_vars vars;

	/* The rest is server-specific */
	sol::state& lua;

	server_step_type current_simulation_step = 0;

	augs::serialization_buffers buffers;

	entropy_accumulator local_collected;
	server_step_entropy step_collected;

	augs::propagate_const<std::unique_ptr<server_adapter>> server;
	std::array<server_client_state, max_incoming_connections_v> clients;
	unsigned ticks_until_sending_packets = 0;
	net_time_t server_time = 0.0;

	/* No server state follows later in code. */

	augs::ref_memory_stream make_serialization_stream();

	static net_time_t get_current_time();

	template <class H, class S>
	static decltype(auto) get_arena_handle_impl(S& self) {
		return H {
			self.current_mode,
			self.scene,
			self.scene.world,
			self.rulesets,
			self.initial_signi
		};
	}

	void handle_client_messages();
	void advance_clients_state();
	void send_server_step_entropies(const server_step_entropy& total);
	void send_packets_if_its_time();

	void accept_entropy_of_client(
		const mode_player_id,
		const total_client_entropy&
	);

	friend server_adapter;

	template <class T, class F>
	message_handler_result handle_client_message(
		const client_id_type&, 
		F&& read_payload
	);

	void init_client(const client_id_type&);
	void unset_client(const client_id_type&);

	mode_player_id get_admin_player_id() const;

public:
	static constexpr auto loading_strategy = viewables_loading_type::LOAD_ALL;
	static constexpr bool handles_window_input = true;
	static constexpr bool has_additional_highlights = false;

	server_setup(
		sol::state& lua,
		const server_start_input&,
		const server_vars& 
	);

	~server_setup();

	static mode_player_id to_mode_player_id(const client_id_type&);

	const auto& get_viewed_cosmos() const {
		return scene.world;
	}

	auto get_interpolation_ratio() const {
		const auto dt =  get_viewed_cosmos().get_fixed_delta().in_seconds<double>();
		return (get_current_time() - server_time) / dt;
	}

	entity_id get_viewed_character_id() const;

	auto get_viewed_character() const {
		return get_viewed_cosmos()[get_viewed_character_id()];
	}

	const auto& get_viewable_defs() const {
		return scene.viewables;
	}

	void customize_for_viewing(config_lua_table&) const;

	void apply(const config_lua_table&);
	void apply(const server_vars&, bool force);

	void choose_arena(const std::string& name);

	std::string describe_client(const client_id_type id) const;
	void log_malicious_client(const client_id_type id);

	double get_audiovisual_speed() const;
	double get_inv_tickrate() const;

	template <class C>
	void advance(
		const setup_advance_input& in,
		const C& callbacks
	) {
		const auto current_time = get_current_time();

		while (server_time <= current_time) {
			step_collected.clear();

			handle_client_messages();
			advance_clients_state();

			/* Extract entropy from the built-in server player */
			{
				const auto admin_entropy = local_collected.extract(
					get_viewed_character(), 
					get_admin_player_id(),
					{ in.settings, in.screen_size }
				);

				step_collected += admin_entropy;
			}

			send_server_step_entropies(step_collected);
			send_packets_if_its_time();

			get_arena_handle().advance(step_collected, callbacks);

			++current_simulation_step;
			server_time += get_inv_tickrate();

			step_collected.clear();
		}
	}

	template <class T>
	void control(const T& t) {
		local_collected.control(t);
	}

	void accept_game_gui_events(const game_gui_entropy_type&);

	augs::path_type get_unofficial_content_dir() const {
		return {};
	}

	auto get_render_layer_filter() const {
		return render_layer_filter::disabled();
	}

	void ensure_handler() {}

	online_arena_handle<false> get_arena_handle();
	online_arena_handle<true> get_arena_handle() const;

	void disconnect_and_unset(const client_id_type&);

	mode_player_id get_local_player_id() const {
		return get_admin_player_id();
	}
};
