#pragma once
#include "augs/math/camera_cone.h"
#include "game/detail/render_layer_filter.h"
#include "application/setups/client/client_start_input.h"
#include "application/intercosm.h"
#include "game/cosmos/cosmos.h"
#include "game/cosmos/entity_handle.h"

#include "application/setups/default_setup_settings.h"
#include "application/input/entropy_accumulator.h"

#include "application/setups/setup_common.h"
#include "application/setups/server/server_vars.h"

#include "application/network/network_common.h"

#include "augs/network/network_types.h"
#include "application/setups/server/server_vars.h"

#include "application/predefined_rulesets.h"
#include "application/arena/mode_and_rules.h"
#include "augs/readwrite/memory_stream_declaration.h"
#include "augs/misc/serialization_buffers.h"

#include "view/mode_gui/arena/arena_gui_mixin.h"
#include "view/audiovisual_state/audiovisual_post_solve_settings.h"

#include "application/network/client_state_type.h"
#include "application/network/requested_client_settings.h"

#include "application/network/simulation_receiver.h"
#include "application/session_profiler.h"
#include "application/setups/client/lag_compensation_settings.h"

#include "augs/misc/getpid.h"
#include "game/modes/dump_for_debugging.h"

#include "application/network/special_client_request.h"

struct config_lua_table;

class client_adapter;

enum class client_arena_type {
	PREDICTED,
	REFERENTIAL
};

class client_setup : 
	public default_setup_settings,
	public arena_gui_mixin<client_setup>
{
	using arena_base = arena_gui_mixin<client_setup>;
	friend arena_base;
	friend client_adapter;

	/* This is loaded from the arena folder */
	intercosm scene;
	cosmos_solvable_significant initial_signi;

	predefined_rulesets rulesets;

	/* Other replicated state */
	online_mode_and_rules current_mode;
	server_vars sv_vars;

	mode_player_id client_player_id;

	cosmos predicted_cosmos;
	online_mode_and_rules predicted_mode;

	special_client_request pending_request = special_client_request::NONE;
	bool now_resyncing = false;

	/* The rest is client-specific */
	sol::state& lua;

	simulation_receiver receiver;

	client_start_input last_start;
	client_state_type state = client_state_type::INVALID;

	client_vars vars;
	requested_client_settings requested_settings;
	bool resend_requested_settings = false;

	entropy_accumulator total_collected;
	augs::serialization_buffers buffers;

	augs::propagate_const<std::unique_ptr<client_adapter>> client;
	net_time_t client_time = 0.0;
	net_time_t when_initiated_connection = 0.0;

	double default_inv_tickrate = 1 / 128.0;

	std::string last_disconnect_reason;

	/* No client state follows later in code. */

	static net_time_t get_current_time();

	template <class H, class S>
	static decltype(auto) get_arena_handle_impl(S& self, const client_arena_type t) {
		if (t == client_arena_type::PREDICTED) {
			return H {
				self.predicted_mode,
				self.scene,
				self.predicted_cosmos,
				self.rulesets,
				self.initial_signi
			};
		}
		else {
			ensure_eq(t, client_arena_type::REFERENTIAL);

			return H {
				self.current_mode,
				self.scene,
				self.scene.world,
				self.rulesets,
				self.initial_signi
			};
		}
	}

	void handle_server_messages();
	void send_pending_commands();
	void send_packets_if_its_time();

	template <class T, class F>
	message_handler_result handle_server_message(
		F&& read_payload
	);

	void send_to_server(total_client_entropy&);

	client_arena_type get_viewed_arena_type() const;

public:
	static constexpr auto loading_strategy = viewables_loading_type::LOAD_ALL;
	static constexpr bool handles_window_input = true;
	static constexpr bool has_additional_highlights = false;

	client_setup(
		sol::state& lua,
		const client_start_input&
	);

	~client_setup();

	void init_connection(const client_start_input&);

	const cosmos& get_viewed_cosmos() const;

	auto get_interpolation_ratio() const {
		const auto dt = get_viewed_cosmos().get_fixed_delta().in_seconds<double>();
		return std::min(1.0, (get_current_time() - client_time) / dt);
	}

	entity_id get_viewed_character_id() const;

	auto get_viewed_character() const {
		return get_viewed_cosmos()[get_viewed_character_id()];
	}

	const auto& get_viewable_defs() const {
		return scene.viewables;
	}

	custom_imgui_result perform_custom_imgui(perform_custom_imgui_input);

	void customize_for_viewing(config_lua_table&) const;

	void apply(const config_lua_table&);

	double get_audiovisual_speed() const;
	double get_inv_tickrate() const;

	template <class Callbacks>
	void advance(
		const client_advance_input& in,
		const Callbacks& callbacks
	) {
		const auto referential_solve_settings = [&]() {
			solve_settings out;
			out.effect_prediction = in.lag_compensation.effect_prediction;
			return out;
		}();

		const auto predicted_solve_settings = [&]() {
			solve_settings out;
			out.effect_prediction = in.lag_compensation.effect_prediction;

			if (in.lag_compensation.confirm_controlled_character_death) {
				out.disable_knockouts = get_viewed_character();
			}

			return out;
		}();

		auto schedule_reprediction_if_inconsistent = [&](const auto result) {
			if (result.state_inconsistent) {
				receiver.schedule_reprediction = true;
			}
		};

		auto& performance = in.network_performance;

		const auto current_time = get_current_time();

		if (client_time <= current_time) {
			{
				auto scope = measure_scope(performance.receiving_messages);
				handle_server_messages();
			}

			const auto new_local_entropy = [&]() -> std::optional<mode_entropy> {
				if (is_gameplay_on()) {
					return total_collected.extract(
						get_viewed_character(), 
						get_local_player_id(), 
						in.make_accumulator_input()
					);
				}

				return std::nullopt;
			}();

			const bool in_game = new_local_entropy != std::nullopt;

			if (is_connected()) {
				auto scope = measure_scope(performance.sending_messages);

				send_pending_commands();

				if (in_game) {
					auto new_client_entropy = new_local_entropy->get_for(
						get_viewed_character(), 
						get_local_player_id()
					);

					send_to_server(new_client_entropy);
				}
			}

			{
				auto scope = measure_scope(performance.sending_packets);
				send_packets_if_its_time();
			}

			if (in_game) {
				auto referential_arena = get_arena_handle(client_arena_type::REFERENTIAL);
				auto predicted_arena = get_arena_handle(client_arena_type::PREDICTED);

				auto audiovisual_post_solve = callbacks.post_solve;

				auto for_each_effect_queue = [&](const const_logic_step step, auto callback) {
					namespace M = messages;

					auto& q = step.transient.messages;

					callback(q.get_queue<M::start_particle_effect>());
					callback(q.get_queue<M::stop_particle_effect>());

					callback(q.get_queue<M::start_sound_effect>());
					callback(q.get_queue<M::stop_sound_effect>());
					callback(q.get_queue<M::start_multi_sound_effect>());

					callback(q.get_queue<M::thunder_effect>());
					callback(q.get_queue<M::exploding_ring_effect>());
				};

				{
					auto scope = measure_scope(performance.unpacking_remote_steps);

					auto referential_post_solve = [&](const const_logic_step step) {
						const auto input = prediction_input::unpredictable_for(get_viewed_character());

						auto erase_predictable_messages = [&](auto& from_queue) {
							erase_if(
								from_queue,
								[&](const auto& m) {
									return !m.get_predictability().should_play(input);
								}
							);
						};

						for_each_effect_queue(step, erase_predictable_messages);

						audiovisual_post_solve_settings settings;
						settings.prediction = input;

						audiovisual_post_solve(step, settings);
					};

					auto referential_callbacks = solver_callbacks(
						default_solver_callback(),
						referential_post_solve,
						default_solver_callback()
					);

					auto advance_referential = [&](const auto& entropy) {
						referential_arena.advance(entropy, referential_callbacks, referential_solve_settings);
					};

					auto advance_repredicted = [&](const auto& entropy) {
						/* 
							Note that we do not post-solve for the re-simulation process. 
							We only post-solve once for the predicted cosmos, when we actually move forward in time.
						*/

						const auto reprediction_result = predicted_arena.advance(
							entropy, 
							solver_callbacks(), 
							predicted_solve_settings
						);

						schedule_reprediction_if_inconsistent(reprediction_result);
					};

					auto unpack = [&](const compact_server_step_entropy& entropy) {
						auto mode_id_to_entity_id = [&](const mode_player_id& mode_id) {
							return get_arena_handle(client_arena_type::REFERENTIAL).on_mode(
								[&](const auto& typed_mode) {
									return typed_mode.lookup(mode_id);
								}
							);
						};

						return entropy.unpack(mode_id_to_entity_id);
					};

					const auto result = receiver.unpack_deterministic_steps(
						in.simulation_receiver,
						in.interp,
						in.past_infection,

						get_viewed_character(),

						unpack,

						referential_arena,
						predicted_arena,

						advance_referential,
						advance_repredicted
					);

					performance.accepted_commands.measure(result.total_accepted);

					if (result.malicious_server) {
						LOG("There was a problem unpacking steps from the server. Disconnecting.");
						log_malicious_server();
						disconnect();
					}

					if (result.desync && !now_resyncing) {
						pending_request = special_client_request::RESYNC;
						now_resyncing = true;

#if DUMP_BEFORE_AND_AFTER_ROUND_START
						const auto preffix = typesafe_sprintf("%x_desync%x_", augs::getpid(), referential_arena.get_round_num());

						referential_arena.on_mode(
							[&](const auto& mode) {
								::dump_for_debugging(
									lua,
									preffix,
									referential_arena.get_cosmos(),
									mode
								);
							}
						);
#endif

#if DISCONNECT_ON_DESYNC
						last_disconnect_reason = typesafe_sprintf(
							"The client has desynchronized from the server."
						);

						disconnect();
#endif
					}
				}

				{
					auto scope = measure_scope(performance.stepping_forward);

					{
						auto& p = receiver.predicted_entropies;

						const auto& max_commands = vars.max_predicted_client_commands;
						const auto num_commands = p.size();

						if (num_commands > max_commands) {
							last_disconnect_reason = typesafe_sprintf(
								"Number of predicted commands (%x) exceeded max_predicted_client_commands (%x).", 
								num_commands,
								max_commands
							);

							disconnect();
						}

						performance.predicted_steps.measure(num_commands);

						p.push_back(*new_local_entropy);
					}

					auto predicted_post_solve = [&](const const_logic_step step) {
						const auto input = prediction_input::predictable_for(get_viewed_character());

						auto erase_unpredictable_messages = [&](auto& from_queue) {
							erase_if(
								from_queue,
								[&](const auto& m) {
									return !m.get_predictability().should_play(input);
								}
							);
						};

						for_each_effect_queue(step, erase_unpredictable_messages);

						audiovisual_post_solve_settings settings;
						settings.prediction = input;

						audiovisual_post_solve(step, settings);
					};

					auto predicted_callbacks = solver_callbacks(
						default_solver_callback(),
						predicted_post_solve,
						default_solver_callback()
					);

#if USE_CLIENT_PREDICTION
					const auto forward_step_result = predicted_arena.advance(
						*new_local_entropy, 
						predicted_callbacks, 
						predicted_solve_settings
					);

					schedule_reprediction_if_inconsistent(forward_step_result);
#else
					(void)predicted_callbacks;
					(void)callbacks;
#endif
				}
			}

			if (in_game) {
				client_time += get_inv_tickrate();
			}
			else {
				client_time += default_inv_tickrate;
			}

			update_stats(in.network_stats);
			total_collected.clear();
		}
	}

	template <class T>
	void control(const T& t) {
		total_collected.control(t);
	}

	void accept_game_gui_events(const game_gui_entropy_type&);

	augs::path_type get_unofficial_content_dir() const;

	auto get_render_layer_filter() const {
		return render_layer_filter::disabled();
	}

	void ensure_handler() {}

	mode_player_id get_local_player_id() const {
		return client_player_id;
	}

	online_arena_handle<false> get_arena_handle(std::optional<client_arena_type> = std::nullopt);
	online_arena_handle<true> get_arena_handle(std::optional<client_arena_type> = std::nullopt) const;

	void log_malicious_server();

	bool is_connected() const;
	void disconnect();

	bool is_gameplay_on() const;
	setup_escape_result escape();

	void update_stats(network_info&) const;
	void draw_custom_gui(const draw_setup_gui_input& in) const;
};
