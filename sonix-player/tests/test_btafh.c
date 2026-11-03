#include <assert.h>
#include <stdio.h>

#include "src/system/bluetooth/btafh.h"

static int bit(const uint8_t *map, int channel) {
	return (map[channel / 8] >> (channel % 8)) & 1;
}

static int usable_channels(const uint8_t *map) {
	int count = 0;
	for (int channel = 0; channel < 79; channel++) {
		count += bit(map, channel);
	}
	return count;
}

int main(void) {
	uint8_t map[10];

	// Wi-Fi channel 6 at 2437 MHz masks Bluetooth channels 23 through 47.
	int masked = btafh_map_for_wifi(2437, map);
	assert(masked == 25);
	assert(usable_channels(map) == 54);
	assert(bit(map, 22) && !bit(map, 23) && !bit(map, 47) && bit(map, 48));
	assert(map[9] == 0x7f); // Bluetooth channel 79 does not exist.

	// The channel 1 mask is clipped at the bottom of the Bluetooth band.
	masked = btafh_map_for_wifi(2412, map);
	assert(masked == 23 && !bit(map, 0) && !bit(map, 22) && bit(map, 23));

	// The channel 13 mask is clipped at the top and keeps enough channels.
	masked = btafh_map_for_wifi(2472, map);
	assert(masked == 21 && !bit(map, 78) && !bit(map, 58) && bit(map, 57));
	assert(usable_channels(map) >= 20);

	// Unknown and 5 GHz frequencies leave every Bluetooth channel available.
	for (int frequency = 0; frequency < 2; frequency++) {
		masked = btafh_map_for_wifi(frequency ? 5180 : 0, map);
		assert(masked == 0 && usable_channels(map) == 79);
	}

	puts("PASS btafh: Wi-Fi channels are excluded and the remaining map is valid");
	return 0;
}
