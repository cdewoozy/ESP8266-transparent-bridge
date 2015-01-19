#include "espmissingincludes.h"
#include "config.h"
#include "flash_param.h"
#include <string.h>
#include <spi_flash.h>
#include <osapi.h>
#include <c_types.h>
#include <ets_sys.h>
#include <user_interface.h>

#define FLASH_PARAM_START_SECTOR 0x3C
#define FLASH_PARAM_SECTOR (FLASH_PARAM_START_SECTOR + 0)
#define FLASH_PARAM_ADDR (SPI_FLASH_SEC_SIZE * FLASH_PARAM_SECTOR)

static int flash_param_loaded = 0;
flash_param_t flash_param;

static void ICACHE_FLASH_ATTR flash_param_init_defaults(void)
{
	struct station_config sta_conf;
	struct softap_config ap_conf;
	flash_param.magic = FLASH_PARAM_MAGIC;
	flash_param.version = FLASH_PARAM_VERSION;
	flash_param.baud = 115200;
	flash_param.port = 23;
	os_strncpy(flash_param.sta_ssid, STA_SSID, sizeof(flash_param.sta_ssid));
	os_strncpy(flash_param.sta_passwd, STA_PASSWORD, sizeof(flash_param.sta_passwd));
	os_strncpy(flash_param.ap_ssid, AP_SSID, sizeof(flash_param.ap_ssid));
	os_strncpy(flash_param.ap_passwd, AP_PASSWORD, sizeof(flash_param.ap_passwd));
	flash_param_set();
}

static void ICACHE_FLASH_ATTR flash_param_read()
{
	spi_flash_read(FLASH_PARAM_ADDR, (uint32 *)&flash_param, sizeof(flash_param_t));
	if (flash_param.magic != FLASH_PARAM_MAGIC || flash_param.version != FLASH_PARAM_VERSION) {
		flash_param_init_defaults();
	}
}

static void ICACHE_FLASH_ATTR flash_param_write()
{
	ETS_UART_INTR_DISABLE();
	spi_flash_erase_sector(FLASH_PARAM_SECTOR);
	spi_flash_write(FLASH_PARAM_ADDR, (uint32 *)&flash_param, sizeof(flash_param));
	ETS_UART_INTR_ENABLE();
}

flash_param_t *ICACHE_FLASH_ATTR flash_param_get(void)
{
	if (!flash_param_loaded) {
		flash_param_read();
		flash_param_loaded = 1;
	}
	return &flash_param;
}

int ICACHE_FLASH_ATTR flash_param_set(void)
{
	flash_param_write(&flash_param);
	flash_param_t tmp;
	flash_param_read(&tmp);
	if (memcmp(&tmp, &flash_param, sizeof(flash_param_t)) != 0) {
		return 0;
	}
	return 1;
}

