#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include "user_config.h"


const char *FlashSizeMap[] =
{
		"512 KB (256 KB + 256 KB)",		// 0x00
		"256 KB",						// 0x01
		"1024 KB (512 KB + 512 KB)", 	// 0x02
		"2048 KB (512 KB + 512 KB)"		// 0x03
		"4096 KB (512 KB + 512 KB)"		// 0x04
		"2048 KB (1024 KB + 1024 KB)"	// 0x05
		"4096 KB (1024 KB + 1024 KB)"	// 0x06
};

void ICACHE_FLASH_ATTR print_info() {
	INFO("\r\n\r\n");
	INFO("BOOTUP...\r\n");
	INFO("SDK version:%s rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check());
	INFO("Time = %ld\n", system_get_time());
	INFO("Chip ID: %08X\r\n", system_get_chip_id());
	INFO("CPU freq: %d MHz\r\n", system_get_cpu_freq());
	INFO("Flash size map [%d]: %s\r\n", system_get_flash_size_map(), FlashSizeMap[system_get_flash_size_map()]);
	INFO("Free heap size: %d\r\n", system_get_free_heap_size());
	INFO("Memory info:\r\n");
	system_print_meminfo();
	INFO("-------------------------------------------\r\n");
	INFO("%s v.%d.%d.%d \r\n", APP_NAME, APP_VER_MAJ, APP_VER_MIN, APP_VER_REV);
	INFO("compile time:%s %s\r\n",__DATE__,__TIME__);
	INFO("-------------------------------------------\r\n");
}
