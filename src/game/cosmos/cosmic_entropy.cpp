#include "augs/templates/container_templates.h"
#include "augs/templates/introspect.h"
#include "cosmic_entropy.h"
#include "game/cosmos/cosmos.h"
#include "game/cosmos/entity_handle.h"

#include "augs/misc/machine_entropy.h"
#include "augs/templates/introspection_utils/rewrite_members.h"

#include "game/detail/inventory/perform_transfer.h"

template <class key>
std::size_t basic_cosmic_entropy<key>::length() const {
	std::size_t total = 0;

	for (const auto& p : players) {
		const auto& e = p.second;

		total += e.motions.size();
		total += e.intents.size();
		total += e.transfers.size();

		if (e.cast_spell.is_set()) {
			++total;
		}

		if (e.wield != std::nullopt) { 
			++total;
		}
	}

	return total;
}

template <class key>
bool basic_cosmic_entropy<key>::empty() const {
	return players.empty() || length() == 0;
}

template <class key>
basic_cosmic_entropy<key>& basic_cosmic_entropy<key>::operator+=(const basic_cosmic_entropy& b) {
	for (const auto& p : b.players) {
		const auto& r = p.second;

		auto& l = players[p.first];

		concatenate(l.intents, r.intents);
		concatenate(l.motions, r.motions);
		concatenate(l.transfers, r.transfers);

		if (r.wield != std::nullopt) {
			l.wield = r.wield;
		}

		if (r.cast_spell.is_set()) {
			l.cast_spell = r.cast_spell;
		}
	}

	return *this;
}

template <class key>
void basic_cosmic_entropy<key>::clear() {
	players.clear();
}

template <class key>
void basic_cosmic_entropy<key>::clear_dead_entities(const cosmos& cosm) {
	erase_if(players, [&](auto& p) {
		if (cosm[p.first].dead()) {
			return true;
		}

		auto eraser = [&cosm](const auto& it) {
			return cosm[it.item].dead();
		};

		erase_if(p.second.transfers, eraser);

		return false;
	});
}

template <class K>
bool basic_player_entropy<K>::operator==(const basic_player_entropy<K>& b) const {
	return
		intents == b.intents
		&& motions == b.motions
		&& wield == b.wield
		&& cast_spell == b.cast_spell
		&& transfers == b.transfers
	;
}

template <class K>
bool basic_player_entropy<K>::operator!=(const basic_player_entropy<K>& b) const {
	return !operator==(b);
}

template <class K>
bool basic_cosmic_entropy<K>::operator==(const basic_cosmic_entropy<K>& b) const {
	return players == b.players;
}

template <class K>
bool basic_cosmic_entropy<K>::operator!=(const basic_cosmic_entropy<K>& b) const {
	return !operator==(b);
}

cosmic_entropy::cosmic_entropy(
	const entity_id controlled_entity,
	const game_intents& intents,
	const game_motions& motions
) {
	auto& p = players[controlled_entity];
	p.intents = intents;
	p.motions = motions;
}

template struct basic_cosmic_entropy<entity_id>;
template struct basic_player_entropy<entity_id>;
