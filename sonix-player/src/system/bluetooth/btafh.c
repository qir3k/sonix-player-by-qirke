#include "btafh.h"

#include <string.h>

#define BT_CHANNELS 79
#define WIFI_HALF_WIDTH_MHZ 12

#ifndef HOST_BUILD
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

// Kernel UAPI constants, kept local because the target image omits Bluetooth
// headers and the cross-build does not include them.
#define BT_AF_BLUETOOTH 31
#define BT_PROTO_HCI 1
#define BT_HCI_CHANNEL_RAW 0
#define BT_HCI_COMMAND_PKT 0x01
#define BT_OP_SET_AFH_HOST_CLASS ((0x03 << 10) | 0x003F)

struct bt_sockaddr_hci {
	unsigned short hci_family;
	unsigned short hci_dev;
	unsigned short hci_channel;
};
#endif

int btafh_map_for_wifi(int wifi_freq_mhz, uint8_t map[10]) {
	int masked = 0;
	memset(map, 0, 10);

	for (int channel = 0; channel < BT_CHANNELS; channel++) {
		int frequency = 2402 + channel;
		bool under_wifi = wifi_freq_mhz >= 2400 && wifi_freq_mhz <= 2500 &&
						  frequency >= wifi_freq_mhz - WIFI_HALF_WIDTH_MHZ &&
						  frequency <= wifi_freq_mhz + WIFI_HALF_WIDTH_MHZ;
		if (under_wifi) {
			masked++;
		} else {
			map[channel / 8] |= (uint8_t)(1u << (channel % 8));
		}
	}

	// Bluetooth requires at least twenty channels to remain available.
	if (BT_CHANNELS - masked < 20) {
		memset(map, 0, 10);
		for (int channel = 0; channel < BT_CHANNELS; channel++) {
			map[channel / 8] |= (uint8_t)(1u << (channel % 8));
		}
		return 0;
	}
	return masked;
}

bool btafh_apply_for_wifi(int wifi_freq_mhz) {
#ifdef HOST_BUILD
	(void)wifi_freq_mhz;
	return false;
#else
	uint8_t packet[14];
	uint8_t map[10];
	int masked = btafh_map_for_wifi(wifi_freq_mhz, map);

	int fd = socket(BT_AF_BLUETOOTH, SOCK_RAW, BT_PROTO_HCI);
	if (fd < 0) {
		return false;
	}
	(void)fcntl(fd, F_SETFD, FD_CLOEXEC);

	struct bt_sockaddr_hci address = {BT_AF_BLUETOOTH, 0, BT_HCI_CHANNEL_RAW};
	if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
		close(fd);
		return false;
	}

	packet[0] = BT_HCI_COMMAND_PKT;
	packet[1] = (uint8_t)(BT_OP_SET_AFH_HOST_CLASS & 0xff);
	packet[2] = (uint8_t)(BT_OP_SET_AFH_HOST_CLASS >> 8);
	packet[3] = sizeof(map);
	memcpy(packet + 4, map, sizeof(map));

	ssize_t written = write(fd, packet, sizeof(packet));
	int saved_errno = errno;
	close(fd);

	if (written != (ssize_t)sizeof(packet)) {
		fprintf(stderr, "bluetooth: AFH classification not sent (%s)\n", strerror(saved_errno));
		return false;
	}
	if (masked) {
		fprintf(stderr, "bluetooth: AFH: %d channels under Wi-Fi at %d MHz excluded from hopping\n",
				masked, wifi_freq_mhz);
	} else {
		fprintf(stderr, "bluetooth: AFH: all channels allowed\n");
	}
	return true;
#endif
}
