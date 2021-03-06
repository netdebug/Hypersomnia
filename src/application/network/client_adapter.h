#pragma once
#include "augs/global_libraries.h"
#include "application/network/network_adapters.h"
#include "augs/network/network_simulator_settings.h"

class client_adapter {
	augs::network_raii raii;

	std::array<uint8_t, yojimbo::KeyBytes> privateKey = {};
	game_connection_config connection_config;
	GameAdapter adapter;
	yojimbo::Client client;

	friend GameAdapter;

	template <class H>
	message_handler_result process_message(yojimbo::Message&, H&& handler);

	template <class T>
	auto create_message();

public:
	client_adapter();
	void connect(const client_start_input&);

	template <class H>
	void advance(
		const net_time_t client_time, 
		H&& handler
	);

	void send_packets();

	bool can_send_message(const game_channel_type&) const;
	bool has_messages_to_send(const game_channel_type&) const;

	template <class... Args>
	bool send_payload(
		const game_channel_type& channel_id, 
		Args&&... args
	);

	bool is_disconnected() const;
	bool is_connected() const;
	bool is_connecting() const;
	bool has_connection_failed() const;

	void disconnect();

	auto& get_specific() {
		return client;
	}

	const auto& get_specific() const {
		return client;
	}

	void set(augs::maybe_network_simulator);
	network_info get_network_info() const;
};
