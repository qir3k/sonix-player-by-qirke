#ifndef BTAFH_H
#define BTAFH_H

#include <stdbool.h>
#include <stdint.h>

// Bluetooth's AFH map avoids the channels covered by a joined 2.4 GHz Wi-Fi
// network. Bit n is Bluetooth channel n (2402 + n MHz); 1 means usable.
// Unknown and 5 GHz frequencies leave all Bluetooth channels enabled.
int btafh_map_for_wifi(int wifi_freq_mhz, uint8_t map[10]);

// Sends the AFH map to hci0. The return value reports whether the command was
// written to the controller, not whether the controller accepted it.
bool btafh_apply_for_wifi(int wifi_freq_mhz);

#endif
