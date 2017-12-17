#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include <c_types.h>
#define USE_OPTIMIZE_PRINTF

//Mqtt settings
#define MQTT_HOST     			"192.168.13.100"
#define MQTT_PORT     			1883
#define MQTT_KEEPALIVE    		30  /*second*/
#define MQTT_RECONNECT_TIMEOUT  10  /*second*/
#define MQTT_CLEAN_SESSION 		1
#define MQTT_BUF_SIZE   		1024
#define MQTT_CLIENT_ID    		"ESP"

#define PROTOCOL_NAMEv311

#define MQTT_TOPIC_BASE		"/angst/devices"
#define MQTT_DISCOVER		MQTT_TOPIC_BASE"/discovery/"
#define MQTT_ABOUT			"about"
#define MQTT_STATUS    		"status"
#define MQTT_CLIENT_TYPE    "relay"
#define MQTT_RELAY_KEY   	"states"
#define MQTT_SWITCH			"switch"
#define MQTT_SWITCH_ON		"on"
#define MQTT_SWITCH_OFF		"off"


//Blinky
#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

//GPIO expander
#define PCF8574_SENSE_READ 	0x41
#define PCF8574_LED_WRITE 	0x44
#define PCF8574_RELAY_WRITE 0x48

//I2C pins
#define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_MTDI_U
#define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_MTCK_U
#define I2C_MASTER_SDA_GPIO 12
#define I2C_MASTER_SCL_GPIO 13
#define I2C_MASTER_SDA_FUNC FUNC_GPIO12
#define I2C_MASTER_SCL_FUNC FUNC_GPIO13




#define ERROR_LEVEL
#define WARN_LEVEL
#define INFO_LEVEL
//#define DEBUG_LEVEL

#define APP_NAME        "Relay"
#define APP_VER_MAJ		1
#define APP_VER_MIN		0
#define APP_VER_REV		0

#define LOCAL_CONFIG_AVAILABLE

#ifndef LOCAL_CONFIG_AVAILABLE
#error Please copy user_config.sample.h to user_config.local.h and modify your configurations
#else
#include "user_config.local.h"
#endif


#ifdef ERROR_LEVEL
#define ERROR( format, ... ) os_printf( "[ERROR] " format, ## __VA_ARGS__ )
#else
#define ERROR( format, ... )
#endif

#ifdef WARN_LEVEL
#define WARN( format, ... ) os_printf( "[WARN] " format, ## __VA_ARGS__ )
#else
#define WARN( format, ... )
#endif

#ifdef INFO_LEVEL
#define MQTT_DEBUG_ON
#define INFO( format, ... ) os_printf( "[INFO] " format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif

#ifdef DEBUG_LEVEL
#define DEBUG( format, ... ) os_printf( "[DEBUG] " format, ## __VA_ARGS__ )
#else
#define DEBUG( format, ... )
#endif


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


#endif
