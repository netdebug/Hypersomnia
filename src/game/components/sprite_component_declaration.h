#pragma once
#include "game/assets/ids/game_image_id.h"

namespace augs {
	template <class id_type>
	struct sprite;
}

namespace components {
	using sprite = augs::sprite<assets::game_image_id>;
}