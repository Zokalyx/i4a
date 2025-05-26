#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"

#include "config.h"
#include "utils.h"
#include "wifi.h"
#include "ring_link.h"
#include "device.h"
#include "client.h"

static const char *TAG = "==> main";

void app_main(void)
{
	// Reads the GPIO pins to determine:
	// - Whether the device is root or not
	// - The orientation of the device (N S W E, or Center)
	// NOTE: The config is a static struct, that's why we don't need to declare a variable here.
	// This also blocks and waits until the center device shares its MAC address through the ring link.
	config_setup();
	config_print();

	Device device;

	config_orientation_t device_orientation = config_get_orientation();
	uint8_t *center_mac = config_get_center_mac();
	const char *device_uuid = sprintf("%02X:%02X:%02X:%02X:%02X:%02X",
					  center_mac[0], center_mac[1],
					  center_mac[2], center_mac[3],
					  center_mac[4], center_mac[5]);

	const char *wifi_ssid_prefix;
	const char *wifi_password;
	if (device_orientation == CONFIG_ORIENTATION_NONE) {
		// These are the center devices, which will always act as
		// access points for the end user. We need to avoid using the same
		// prefix as the rest of the network (I4A) to prevent unwanted wifi connections.
		wifi_ssid_prefix = "Internet 4 All";
		wifi_password = "";
		Device_Mode mode = AP;
	} else {
		wifi_ssid_prefix = "I4A";
		wifi_password =
			"test123456"; // Should we move this away from tracked files?
		// All side ESPs start out as stations, until another one connects and everyone else
		// switches to AP mode.
		Device_Mode mode = STATION;
	}

	uint8_t ap_channel_to_emit = 6; // Hardcoded?
	uint8_t ap_max_sta_connections = 4; // Why 4?
	uint8_t device_is_root = config_get_mode() == CONFIG_MODE_ROOT ? 1 : 0;

	ESP_ERROR_CHECK(wifi_init());

	// We are only doing the ring_link_init here, but we need the MAC of the center
	// device before setting up the WIFI
	ESP_ERROR_CHECK(ring_link_init());

	// How is this determined?
	char *network_cidr = "10.5.6.1";
	char *network_gateway = "10.5.6.1";
	char *network_mask = "255.255.255.0";

	device_start(&device, device_uuid, device_orientation, wifi_ssid_prefix,
		     wifi_password, ap_channel_to_emit, ap_max_sta_connections,
		     device_is_root, mode);

	// When another device on the same node connects, switch to AP mode
	// This should be an EVENT, not a condition.
	if (0) {
		ESP_LOGI(
			TAG,
			"Switching to AP mode due to another device connection");
		mode = AP;
		device_reset(&device);
		device_start(&device, device_uuid, device_orientation,
			     wifi_ssid_prefix, wifi_password,
			     ap_channel_to_emit, ap_max_sta_connections,
			     device_is_root, mode);
	}
}
