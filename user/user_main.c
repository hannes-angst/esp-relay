/*
 The obligatory blinky demo
 Blink an LED on GPIO pin 2
 */

#include <ets_sys.h>
#include "user_interface.h"
#include <osapi.h>
#include <mem.h>
#include <gpio.h>
#include "mqtt.h"
#include "wifi.h"
#include "info.h"
#include "i2c_master.h"


#define BLINK_FREQ   10 /* cycles */
#define INPUT_DELAY 100 /* microseconds */

static ETSTimer input_timer;
static uint8_t state = 0;
static uint8_t last = 0;
static uint8_t round = 0;
static uint8_t relay = 0xFF;

static MQTT_Client mqttClient;

//rotate value
// abcdefgh->hgfedcba
static uint8_t ICACHE_FLASH_ATTR reverse(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

static void ICACHE_FLASH_ATTR set_led(uint8_t value) {
	i2c_master_start();
	i2c_master_writeByte(PCF8574_LED_WRITE);
	if (i2c_master_checkAck()) {
		i2c_master_writeByte(reverse(value));
		if (i2c_master_checkAck()) {
			DEBUG("(%02X) LEDs did ACK [%d].\r\n", round, value);
		} else {
			ERROR("(%02X) LEDs did not ACK [%d].\r\n", round, value);
		}
	} else {
		ERROR("(%02X) LEDs did not ACK.\r\n", round);
	}
	i2c_master_stop();
}

static void ICACHE_FLASH_ATTR set_relay(uint8_t value) {
	i2c_master_start();
	i2c_master_writeByte(PCF8574_RELAY_WRITE);
	if (i2c_master_checkAck()) {
		i2c_master_writeByte(value);
		if (i2c_master_checkAck()) {
			DEBUG("(%02X) Relay did ACK [%d].\r\n", round, value);
		} else {
			ERROR("(%02X) Relay did not ACK [%d].\r\n", round, value);
		}
	} else {
		ERROR("(%02X) Relay did not ACK.\r\n", round);
	}
	i2c_master_stop();
}

static void ICACHE_FLASH_ATTR pubRelayState(uint8_t relayNo, uint8_t relayState) {
	char *topicBuf = (char*) os_zalloc(64);
	char *dataBuf;
	char *id = (char*) os_zalloc(32);

	os_sprintf(id, "%08X", system_get_chip_id());

	//Tell switch status
	os_sprintf(topicBuf, "%s/%s/%s/%d", MQTT_TOPIC_BASE, id, MQTT_SWITCH, relayNo);

	if (relayState == 1) {
		dataBuf = "{\"state\":\""MQTT_SWITCH_OFF"\"}";
	} else {
		dataBuf = "{\"state\":\""MQTT_SWITCH_ON"\"}";
	}

	int len = os_strlen(dataBuf);
	MQTT_Publish(&mqttClient, topicBuf, dataBuf, len, 0, 0);
	os_free(topicBuf);
	os_free(dataBuf);
	os_free(id);
}

//another workaround the port inversion problem
static uint8_t ICACHE_FLASH_ATTR portMapper(uint8_t port) {
	if(port == 1) {
		return 7;
	} else if(port == 2) {
		return 6;
	} else if(port == 3) {
		return 5;
	} else if(port == 4) {
		return 4;
	} else if(port == 5) {
		return 3;
	} else if(port == 6) {
		return 2;
	} else if(port == 7) {
		return 1;
	} else {
		return 0;
	}
}

static void ICACHE_FLASH_ATTR pressed(uint8_t input) {
	INFO("(%02X) Input %d was pressed.\r\n", round, input);
	INFO("was "BYTE_TO_BINARY_PATTERN"\r\n", BYTE_TO_BINARY(relay));
	relay ^= (1 << input);
	INFO("is  "BYTE_TO_BINARY_PATTERN"\r\n", BYTE_TO_BINARY(relay));

	set_relay(relay);
	set_led(relay);
	pubRelayState(portMapper(input + 1) + 1, ((relay >> input) & 0x1));
}

static void ICACHE_FLASH_ATTR switchOn(uint8_t input) {
	if(0 == ((relay >> input)&0x01)) {
		//already switched
		INFO("Input %d already on.\r\n", input);
	} else {
		pressed(input % 8);
	}
}

static void ICACHE_FLASH_ATTR switchOff(uint8_t input) {
	if(1 == ((relay >> input)&0x01)) {
		//already switched
		INFO("Input %d already off.\r\n", input);
	} else {
		pressed(input);
	}
}





static void ICACHE_FLASH_ATTR flipSwitch(const char* topicBuf, bool on) {
	//digit 1..8, ASCII: 49-56
	const uint8_t number = topicBuf[
		os_strlen(MQTT_TOPIC_BASE) + //prefix
		1 + //slash
		8 +//id, e.g. 0016D615
		1 + //slash
		os_strlen(MQTT_SWITCH) + //command
		1] //slash
		- 48;

	if(number > 0 && number < 9) {
		uint8_t port = portMapper(number);
		if(on) {
			switchOn(port);
		} else {
			switchOff(port);
		}
	} else {
		ERROR("Invalid input number: %d\r\n", number);
	}
}


static void checkPressed(uint8_t inpt) {
	uint8_t changed = inpt ^ last;
	DEBUG("Input:   "BYTE_TO_BINARY_PATTERN"\r\n", BYTE_TO_BINARY(inpt)); DEBUG("Last:    "BYTE_TO_BINARY_PATTERN"\r\n", BYTE_TO_BINARY(last));

	if (changed > 0) {
		//check what changed
		DEBUG("Change:  "BYTE_TO_BINARY_PATTERN"\r\n", BYTE_TO_BINARY(changed));
		uint8_t i = 0;
		for (i = 0; i < 8; i++) {
			if ((((changed >> i) & 0x1) == 1) && (((inpt >> i) & 0x1) == 0)) {
				DEBUG("changed[%d]: %d -> %d\r\n", i, ((changed >> i) & 0x1), ((inpt >> i) & 0x1));
				pressed(i);
			}
		}
	}
	last = inpt;
}

static void ICACHE_FLASH_ATTR input_cb(void) {
	uint8_t inpt = 0;
	i2c_master_start();
	i2c_master_writeByte(PCF8574_SENSE_READ);
	if (i2c_master_checkAck()) {
		inpt = i2c_master_readByte();
		if (i2c_master_checkAck()) {
			DEBUG("(%02X) Sense did ACK [%d].\r\n", round, inpt);
		} else {
			ERROR("(%02X) Sense did not ACK [%d].\r\n", round, inpt);
		}
		i2c_master_stop();
		checkPressed(inpt);
	} else {
		i2c_master_stop();
		ERROR("(%02X) Sensors did not ACK.\r\n", round);
	}
}

static void ICACHE_FLASH_ATTR subscribe(MQTT_Client* client) {
	char *topic = (char*) os_zalloc(64);
	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	uint8_t relayNo = 0;
	for (relayNo = 1; relayNo < 9; relayNo++) {
		os_sprintf(topic, "%s/%s/%s/%d/%s", MQTT_TOPIC_BASE, id, MQTT_SWITCH, relayNo, MQTT_SWITCH_ON);
		if (MQTT_Subscribe(client, topic, 0)) {
			INFO("Subscribe from: %s\r\n", topic);
		} else {
			ERROR("Failed to subscribe from %s\r\n", topic);
		}

		os_sprintf(topic, "%s/%s/%s/%d/%s", MQTT_TOPIC_BASE, id, MQTT_SWITCH, relayNo, MQTT_SWITCH_OFF);
		if (MQTT_Subscribe(client, topic, 0)) {
			INFO("Subscribe from: %s\r\n", topic);
		} else {
			ERROR("Failed to subscribe from %s\r\n", topic);
		}
	}

	os_sprintf(topic, "%s/%s/%s", MQTT_TOPIC_BASE, id, MQTT_ABOUT);
	if (MQTT_Subscribe(client, topic, 0)) {
		INFO("Subscribe from: %s\r\n", topic);
	} else {
		ERROR("Failed to subscribe from %s\r\n", topic);
	}

	os_free(id);
	os_free(topic);

	//participate in discovery requests
	if (MQTT_Subscribe(client, MQTT_DISCOVER, 0)) {
		INFO("Subscribe from: %s\r\n", MQTT_DISCOVER);
	} else {
		ERROR("Failed to subscribe from %s\r\n", MQTT_DISCOVER);
	}
}

static void ICACHE_FLASH_ATTR sendDeviceInfo(MQTT_Client* client) {
	char *topicBuf = (char*) os_zalloc(64);
	char *dataBuf = (char*) os_zalloc(265);
	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	//Tell Online status
	os_sprintf(topicBuf, "%s/%s/info", MQTT_TOPIC_BASE, id);
	int len = 0;
	len += os_sprintf(dataBuf + len, "{\"name\":\"Multimedia\"");
	len += os_sprintf(dataBuf + len, ",\"app\":\"%s\"", APP_NAME);
	len += os_sprintf(dataBuf + len, ",\"version\":\"%d.%d.%d\"", APP_VER_MAJ, APP_VER_MIN, APP_VER_REV);
	uint8 mac_addr[6];
	if (wifi_get_macaddr(0, mac_addr)) {
		len += os_sprintf(dataBuf + len, ",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	}
	struct ip_info info;
	if (wifi_get_ip_info(0, &info)) {
		len += os_sprintf(dataBuf + len, ",\"ip\":\"%d.%d.%d.%d\"", IP2STR(&info.ip.addr));
	}
	len += os_sprintf(dataBuf + len, ",\"type\":\"%s\"", MQTT_CLIENT_TYPE);


	len += os_sprintf(dataBuf + len, ",\"%s\": [", MQTT_RELAY_KEY);
	uint8_t relayNo = 0;
	for (relayNo = 8; relayNo > 0; relayNo--) {
		if(((relay >> (relayNo - 1)) & 0x01) == 0)
		{
			len += os_sprintf(dataBuf + len, "\"on\"");
		} else {
			len += os_sprintf(dataBuf + len, "\"off\"");
		}
		if(relayNo > 1) {
			len += os_sprintf(dataBuf + len, ",");
		}
	}
	len += os_sprintf(dataBuf + len, "]");

	len += os_sprintf(dataBuf + len, ",\"base\":\"%s/%s/\"}", MQTT_TOPIC_BASE, id);

	MQTT_Publish(client, topicBuf, dataBuf, len, 0, 0);
	os_free(id);
	os_free(topicBuf);
	os_free(dataBuf);
}

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) {
	INFO("MQTT: Connected\r\n");
	mqttClient = *(MQTT_Client*) args;
	subscribe(&mqttClient);
	sendDeviceInfo(&mqttClient);
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
	MQTT_Client* client = (MQTT_Client*)args;
	char *topicBuf = (char*) os_zalloc(topic_len + 1);
	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	DEBUG("Receive topic: %s, \r\n", topicBuf);
	if (topic_len >= os_strlen(MQTT_ABOUT) && os_strcmp(topicBuf + topic_len - os_strlen(MQTT_ABOUT), MQTT_ABOUT) == 0) {
		sendDeviceInfo(client);
	} else if (os_strcmp(topicBuf, MQTT_DISCOVER) == 0) {
		sendDeviceInfo(client);
	} else if (topic_len >= os_strlen(MQTT_SWITCH_ON) && os_strcmp(topicBuf + topic_len - os_strlen(MQTT_SWITCH_ON), MQTT_SWITCH_ON) == 0) {
		//MQTT_TOPIC_BASE/<id>/MQTT_SWITCH/<number>/MQTT_SWITCH_ON
		flipSwitch(topicBuf, true);
	} else if (topic_len >= os_strlen(MQTT_SWITCH_OFF) && os_strcmp(topicBuf + topic_len - os_strlen(MQTT_SWITCH_OFF), MQTT_SWITCH_OFF) == 0) {
		//MQTT_TOPIC_BASE/<id>/MQTT_SWITCH/<number>/MQTT_SWITCH_OFF
		flipSwitch(topicBuf, false);
	} else {
		INFO("Ignore data\r\n");
	}
	os_free(topicBuf);
}

static void ICACHE_FLASH_ATTR mqtt_init(void) {
	DEBUG("INIT MQTT\r\n");
	//If WIFI is connected, MQTT gets connected (see wifiConnectCb)
	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, SEC_NONSSL);

	char *clientId = (char*) os_zalloc(64);
	os_sprintf(clientId, "%s%08X", MQTT_CLIENT_ID, system_get_chip_id());

	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	//id will be copied by MQTT_InitLWT
	if (!MQTT_InitClient(&mqttClient, clientId, id, id, MQTT_KEEPALIVE, MQTT_CLEAN_SESSION)) {
		ERROR("Could not initialize MQTT client");
	}
	os_free(id);
	os_free(clientId);

	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

}

static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
	if (status == STATION_GOT_IP) {
		MQTT_Connect(&mqttClient);
	}
}


static void ICACHE_FLASH_ATTR ticker_cb(void) {
	os_timer_disarm(&input_timer);
	if (round % BLINK_FREQ == 0) {
		if(state != 0) {
			state = 0;
			GPIO_OUTPUT_SET(LED_GPIO, state);
		}
	} else if (state == 0) {
		if(state != 1) {
			state = 1;
			GPIO_OUTPUT_SET(LED_GPIO, state);
		}
	}
	DEBUG("(%02X) state: %d\r\n", round, state);

	//check_cb();
	input_cb();
	//led_cb();

	round++;
	os_timer_setfn(&input_timer, (os_timer_func_t *) ticker_cb, NULL);
	os_timer_arm(&input_timer, INPUT_DELAY, 0);
}


void ICACHE_FLASH_ATTR app_init(void) {
	print_info();

	mqtt_init();
	WIFI_Connect(STA_SSID, STA_PASS, wifiConnectCb);

	os_timer_disarm(&input_timer);
	os_timer_setfn(&input_timer, (os_timer_func_t *) ticker_cb, NULL);
	os_timer_arm(&input_timer, INPUT_DELAY, 0);

}

void ICACHE_FLASH_ATTR user_init(void) {
	i2c_master_gpio_init();
	// Configure pin as a GPIO
	PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);
	set_relay(relay);
	system_init_done_cb(app_init);

}
