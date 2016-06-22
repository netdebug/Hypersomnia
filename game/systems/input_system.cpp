#include <random>

#include "window_framework/window.h"
#include "game/entity_id.h"
#include "game/cosmos.h"

#include "game/messages/raw_window_input_message.h"

#include "input_system.h"

#include "augs/templates.h"

#include "augs/filesystem/file.h"
#include "augs/filesystem/directory.h"
#include "augs/misc/time.h"

#include "game/components/transform_component.h"
#include "game/components/gun_component.h"


using namespace augs::window;

bool input_system::found_recording() const {
	return augs::file_exists(L"recorded.inputs");
}

void input_system::replay_found_recording() {
	unmapped_intent_player.player.load_recording("recorded.inputs");
	unmapped_intent_player.player.replay();

	crosshair_intent_player.player.load_recording("recorded_crosshair.inputs");
	crosshair_intent_player.player.replay();

	gui_item_transfer_intent_player.player.load_recording("gui_transfers.inputs");
	gui_item_transfer_intent_player.player.replay();
}

void input_system::record_and_save_this_session() {
	augs::create_directory("sessions/");
	augs::create_directory("sessions/" + augs::get_timestamp());

	unmapped_intent_player.player.record("sessions/" + augs::get_timestamp() + "/recorded.inputs");
	crosshair_intent_player.player.record("sessions/" + augs::get_timestamp() + "/recorded_crosshair.inputs");
	gui_item_transfer_intent_player.player.record("sessions/" + augs::get_timestamp() + "/gui_transfers.inputs");
}

bool input_system::is_replaying() const {
	return unmapped_intent_player.player.is_replaying();
}

input_system::input_system(cosmos& parent_cosmos) : processing_system_templated(parent_cosmos),
	unmapped_intent_player(parent_cosmos)
	, crosshair_intent_player(parent_cosmos)
	, gui_item_transfer_intent_player(parent_cosmos)
{
}

void input_system::acquire_new_events_posted_by_drawing_time_systems() {
	unmapped_intent_player.acquire_new_events_posted_by_drawing_time_systems();
	crosshair_intent_player.acquire_new_events_posted_by_drawing_time_systems();
	gui_item_transfer_intent_player.acquire_new_events_posted_by_drawing_time_systems();
}

void input_system::post_all_events_posted_by_drawing_time_systems_since_last_step() {
	unmapped_intent_player.generate_events_for_logic_step();
	crosshair_intent_player.generate_events_for_logic_step();
	gui_item_transfer_intent_player.generate_events_for_logic_step();
}

input_system::context::context() : enabled(true) {
}

void input_system::post_unmapped_intents_from_raw_window_inputs() {
	step.messages.get_queue<messages::unmapped_intent_message>().clear();

	auto& raw_inputs = step.messages.get_queue<messages::raw_window_input_message>();

	if (!active_contexts.empty()) {
		for (auto& it : raw_inputs) {
			auto& state = it.raw_window_input;

			for (auto& context : active_contexts) {
				if (!context.enabled) continue;

				messages::unmapped_intent_message unmapped_intent;
				unmapped_intent.state = state;

				intent_type intent;

				bool found_context_entry = false;

				if (state.key_event == event::NONE) {
					unmapped_intent.pressed_flag = true;

					auto found_intent = context.event_to_intent.find(state.msg);
					if (found_intent != context.event_to_intent.end()) {
						intent = (*found_intent).second;
						found_context_entry = true;
					}
				}
				else if (state.key_event == event::PRESSED || state.key_event == event::RELEASED) {
					unmapped_intent.pressed_flag = state.key_event == event::PRESSED;

					auto found_intent = context.key_to_intent.find(state.key);
					if (found_intent != context.key_to_intent.end()) {
						intent = (*found_intent).second;
						found_context_entry = true;
					}
				}

				if (found_context_entry) {
					unmapped_intent.intent = intent;
					step.messages.post(unmapped_intent);

					break;
				}
			}
		}
	}
}


void input_system::map_unmapped_intents_to_entities() {
	step.messages.get_queue<messages::intent_message>().clear();

	for (auto& unmapped_intent : step.messages.get_queue<messages::unmapped_intent_message>()) {
		messages::intent_message entity_mapped_intent;
		entity_mapped_intent.unmapped_intent_message::operator=(unmapped_intent);

		for (auto it = targets.begin(); it != targets.end(); ++it) {
			entity_mapped_intent.subject = *it;
			step.messages.post(entity_mapped_intent);
		}
	}
}

