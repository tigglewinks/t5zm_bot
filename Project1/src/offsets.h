#pragma once
#include <cstdint>

namespace Zombies {
	uintptr_t A_EntityList = 0x01BFBBF8;
	uintptr_t A_ViewMatrix = 0x00BA6970;

	namespace Offsets {
		uintptr_t o_location = 0x18;
		uintptr_t o_type_test = 0x4; 
		uintptr_t o_type_test2 = 0xFC; // nova: 1103101952, zombie: 1116733440, 1103101952
		uintptr_t o_type_test3 = 0x8;
		uintptr_t o_health = 0x184;
		uintptr_t o_max_health = 0x188;
		uintptr_t o_distance_between = 0x8C;
		uintptr_t o_location2 = 0x104;
		uintptr_t o_location3 = 0x110;
		uintptr_t o_location4 = 0x11C;
		uintptr_t o_entity_type = 0xBE;
	}
}