#include "game/stateless_systems/input_system.h"

#include "game/messages/intent_message.h"
#include "game/messages/motion_message.h"

#include "game/cosmos/logic_step.h"
#include "game/cosmos/data_living_one_step.h"

#include "game/cosmos/entity_handle.h"
#include "game/cosmos/cosmos.h"

void input_system::make_input_messages(const logic_step step) {
	const auto& cosm = step.get_cosmos();

	for (const auto& p : step.get_entropy().players) {
		if (cosm[p.first].dead()) {
			continue;
		}

		for (const auto& intent : p.second.intents) {
			auto msg = messages::intent_message();
			msg.game_intent::operator=(intent);
			msg.subject = p.first;
			step.post_message(msg);
		}

		for (const auto& motion : p.second.motions) {
			auto msg = messages::motion_message();

			const auto type = motion.first;

			msg.motion = type;
			msg.offset = motion.second;
			msg.subject = p.first;
			step.post_message(msg);
		}
	}
}