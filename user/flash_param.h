#ifndef __FLASH_PARAM_H__
#define __FLASH_PARAM_H__

#define FLASH_PARAM_MAGIC	0x8251
#define FLASH_PARAM_VERSION	3

typedef struct _flash_param {
	uint32_t magic;
	uint32_t version;
	uint32_t baud;
	uint32_t port;
	char sta_ssid[32];
	char sta_passwd[64];
	char ap_ssid[32];
	char ap_passwd[64];
} flash_param_t;

flash_param_t *flash_param_get(void);
void flash_param_set();

#endif /* __FLASH_PARAM_H__ */
