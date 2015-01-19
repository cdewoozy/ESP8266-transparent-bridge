#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "config_wifi.h"
#include "flash_param.h"

void config_execute(flash_param_t *param);

typedef struct espconn *p_espconn;

typedef struct config_commands {
	char *command;
	void (*function)(p_espconn conn, uint8_t argc, char *argv[]);
} config_commands_t;


void config_parse(p_espconn conn, char *buf, int len);

#endif /* __CONFIG_H__ */
