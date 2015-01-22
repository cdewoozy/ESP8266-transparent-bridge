// this is the normal build target ESP include set
#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "driver/uart.h"
#include <gpio.h>

#include "config.h"

void config_execute(flash_param_t *param) {
	uint8_t mode;
	uint8_t macaddr[6] = { 0, 0, 0, 0, 0, 0 };
	struct station_config sta_conf;
	struct softap_config ap_conf;

	// make sure the device is in AP and STA combined mode
	mode = wifi_get_opmode();
	if (mode != STATIONAP_MODE) {
		wifi_set_opmode(STATIONAP_MODE);
		os_delay_us(10000);
		system_restart();
	}

	// connect to our station
	os_bzero(&sta_conf, sizeof(sta_conf));
	wifi_station_get_config(&sta_conf);
	os_strncpy(sta_conf.ssid, param->sta_ssid, sizeof(sta_conf.ssid));
	os_strncpy(sta_conf.password, param->sta_passwd, sizeof(sta_conf.password));
	wifi_station_disconnect();
	ETS_UART_INTR_DISABLE();
	wifi_station_set_config(&sta_conf);
	ETS_UART_INTR_ENABLE();
	wifi_station_connect();

	// setup the soft AP
	os_bzero(&ap_conf, sizeof(ap_conf));
	wifi_softap_get_config(&ap_conf);
	wifi_get_macaddr(SOFTAP_IF, macaddr);
	os_strncpy(ap_conf.ssid, param->ap_ssid, sizeof(ap_conf.ssid));
	ap_conf.ssid_len = strlen(ap_conf.ssid);
	os_strncpy(ap_conf.password, param->ap_passwd, sizeof(ap_conf.password));
	//os_snprintf(&ap_conf.password[strlen(AP_PASSWORD)], sizeof(ap_conf.password) - strlen(AP_PASSWORD), "_%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
//	os_sprintf(ap_conf.password + strlen(ap_conf.password), "_%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
	ap_conf.authmode = AUTH_WPA_PSK;
	ap_conf.channel = 6;
	ETS_UART_INTR_DISABLE(); 
	wifi_softap_set_config(&ap_conf);
	ETS_UART_INTR_ENABLE();
}

#define MSG_OK "OK\r\n"
#define MSG_ERROR "ERROR\r\n"
#define MSG_INVALID_CMD "UNKNOWN COMMAND\r\n"

#define MAX_ARGS 12
#define MSG_BUF_LEN 128

char *my_strdup(char *str) {
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if (!(copy = os_malloc((u_int)len)))
		return (NULL);
	os_memcpy(copy, str, len);
	return (copy);
}

char **config_parse_args(char *buf, uint8_t *argc) {
	const char delim[] = " \t";
	char *save, *tok;
	char **argv = (char **)os_malloc(sizeof(char *) * MAX_ARGS);	// note fixed length
	os_memset(argv, 0, sizeof(char *) * MAX_ARGS);

	*argc = 0;
	for (; *buf == ' ' || *buf == '\t'; ++buf); // absorb leading spaces
	for (tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
		argv[*argc] = my_strdup(tok);
		(*argc)++;
		if (*argc == MAX_ARGS) {
			break;
		}
	}
	return argv;
}

void config_parse_args_free(uint8_t argc, char *argv[]) {
	uint8_t i;
	for (i = 0; i <= argc; ++i) {
		if (argv[i])
			os_free(argv[i]);
	}
	os_free(argv);
}

void config_cmd_reset(struct espconn *conn, uint8_t argc, char *argv[]) {
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	system_restart();
}

void config_cmd_baud(struct espconn *conn, uint8_t argc, char *argv[]) {
	flash_param_t *flash_param = flash_param_get();

	if (argc == 0) {
		char buf[MSG_BUF_LEN];
		uint8_t len;
		len = os_sprintf(buf, "BAUD=%d\n", flash_param->baud);
		espconn_sent(conn, buf, len);
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		uint32_t baud = atoi(argv[1]);
		if ((baud > (UART_CLK_FREQ / 16)) || baud == 0) {
			espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
		} else {
			// pump and dump fifo
			while(TRUE) {
				uint32_t fifo_cnt = READ_PERI_REG(UART_STATUS(0)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
				if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) == 0) {
					break;
				}
			}
			os_printf("Set baud to %d\n", baud);
			os_delay_us(1000);
			uart_div_modify(0, UART_CLK_FREQ / baud);
			flash_param->baud = baud;
			flash_param_set();
			espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		}
	}
}

void config_cmd_port(struct espconn *conn, uint8_t argc, char *argv[]) {
	flash_param_t *flash_param = flash_param_get();

	if (argc == 0) {
		char buf[MSG_BUF_LEN];
		uint8_t len;
		len = os_sprintf(buf, "PORT=%d\n", flash_param->port);
		espconn_sent(conn, buf, len);
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		uint32_t port = atoi(argv[1]);
		if (port == 0) {
			espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
		} else {
			if (port != flash_param->port) {
				flash_param->port = port;
				flash_param_set();
				espconn_sent(conn, MSG_OK, strlen(MSG_OK));
				os_delay_us(10000);
				system_restart();
			} else {
				espconn_sent(conn, MSG_OK, strlen(MSG_OK));
			}
		}
	}
// debug
{
	char buf[1024];
	flash_param = flash_param_get();
	os_sprintf(buf, "flash param:\n\tmagic\t%d\n\tversion\t%d\n\tbaud\t%d\n\tport\t%d\n", flash_param->magic, flash_param->version, flash_param->baud, flash_param->port);
	espconn_sent(conn, buf, strlen(buf));
}
}

void config_cmd_mode(struct espconn *conn, uint8_t argc, char *argv[]) {
	uint8_t mode;

	if (argc == 0) {
		char buf[MSG_BUF_LEN];
		uint8_t len;
		len = os_sprintf(buf, "MODE=%d\n", wifi_get_opmode());
		espconn_sent(conn, buf, len);
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else if (argc != 1) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		mode = atoi(argv[1]);
		if (mode >= 0 && mode <= 3) {
			if (wifi_get_opmode() != mode) {
				ETS_UART_INTR_DISABLE();
				wifi_set_opmode(mode);
				ETS_UART_INTR_ENABLE();
				espconn_sent(conn, MSG_OK, strlen(MSG_OK));
				os_delay_us(10000);
				system_restart();
			} else {
				espconn_sent(conn, MSG_OK, strlen(MSG_OK));
			}
		} else {
			espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
		}
	}
}

// spaces are not supported in the ssid or password
void config_cmd_sta(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct station_config sta_conf;

	os_bzero(&sta_conf, sizeof(struct station_config));
	wifi_station_get_config(&sta_conf);

	if (argc == 0) {
		char buf[MSG_BUF_LEN];
		uint8_t len;
		len = os_sprintf(buf, "SSID=%s PASSWORD=%s\n", sta_conf.ssid, sta_conf.password);
		espconn_sent(conn, buf, len);
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else if (argc != 2) {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	} else {
		os_strncpy(sta_conf.ssid, ssid, sizeof(sta_conf.ssid));
		os_strncpy(sta_conf.password, password, sizeof(sta_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		wifi_station_disconnect();
		ETS_UART_INTR_DISABLE(); 
		wifi_station_set_config(&sta_conf);		
		ETS_UART_INTR_ENABLE(); 
		wifi_station_connect();
	}
}

// spaces are not supported in the ssid or password
void config_cmd_ap(struct espconn *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct softap_config ap_conf;

	os_bzero(&ap_conf, sizeof(struct softap_config));
	wifi_softap_get_config(&ap_conf);

	if (argc == 0) {
		char buf[MSG_BUF_LEN];
		uint8_t len;
		len = os_sprintf(buf, "SSID=%s PASSWORD=%s AUTHMODE=%d CHANNEL=%d\n", ap_conf.ssid, ap_conf.password, ap_conf.authmode, ap_conf.channel);
		espconn_sent(conn, buf, len);
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else if (argc == 1) {
		os_strncpy(ap_conf.ssid, ssid, sizeof(ap_conf.ssid));
		os_bzero(ap_conf.password, sizeof(ap_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		ap_conf.authmode = AUTH_OPEN;
		ap_conf.channel = 6;
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&ap_conf);
		ETS_UART_INTR_ENABLE();
	} else if (argc == 2) {
		os_strncpy(ap_conf.ssid, ssid, sizeof(ap_conf.ssid));
		os_strncpy(ap_conf.password, password, sizeof(ap_conf.password));
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
		ap_conf.authmode = AUTH_WPA_PSK;
		ap_conf.channel = 6;
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&ap_conf);
		ETS_UART_INTR_ENABLE();
	} else {
		espconn_sent(conn, MSG_ERROR, strlen(MSG_ERROR));
	}
}

// spaces are not supported in the ssid or password
void io_reset(struct espconn *conn, uint8_t argc, char *argv[]) {
	GPIO_OUTPUT_SET(5, 0);
	os_printf("Reset 5\n");
	os_delay_us(1000000L);
	GPIO_OUTPUT_SET(5, 1);
	espconn_sent(conn, MSG_OK, strlen(MSG_OK));
}

const config_commands_t config_commands[] = { 
		{ "RESET", &config_cmd_reset }, 
		{ "BAUD", &config_cmd_baud },
		{ "PORT", &config_cmd_port },
		{ "MODE", &config_cmd_mode },
		{ "STA", &config_cmd_sta },
		{ "AP", &config_cmd_ap },
		{ "IORST", &io_reset },
		{ NULL, NULL }
	};

void config_parse(struct espconn *conn, char *buf, int len) {
	char *lbuf = (char *)os_malloc(len + 1), **argv;
	uint8_t i, argc;
	// we need a '\0' end of the string
	os_memcpy(lbuf, buf, len);
	lbuf[len] = '\0';
	
	// command echo
	//espconn_sent(conn, lbuf, len);

	// remove any CR / LF
	for (i = 0; i < len; ++i)
		if (lbuf[i] == '\n' || lbuf[i] == '\r')
			lbuf[i] = '\0';

	// verify the command prefix
	if (os_strncmp(lbuf, "+++AT", 5) != 0) {
		return;
	}
	// parse out buffer into arguments
	argv = config_parse_args(&lbuf[5], &argc);
#if 0
// debugging
	{
		uint8_t i;
		size_t len;
		char buf[MSG_BUF_LEN];
		for (i = 0; i < argc; ++i) {
			//len = os_snprintf(buf, MSG_BUF_LEN, "argument %d: '%s'\r\n", i, argv[i]);
			len = os_sprintf(buf, "argument %d: '%s'\r\n", i, argv[i]);
			espconn_sent(conn, buf, len);
		}
	}
// end debugging
#endif
	if (argc == 0) {
		espconn_sent(conn, MSG_OK, strlen(MSG_OK));
	} else {
		argc--;	// to mimic C main() argc argv
		for (i = 0; config_commands[i].command; ++i) {
			if (os_strncmp(argv[0], config_commands[i].command, strlen(argv[0])) == 0) {
				config_commands[i].function(conn, argc, argv);
				break;
			}
		}
		if (!config_commands[i].command)
			espconn_sent(conn, MSG_INVALID_CMD, strlen(MSG_INVALID_CMD));
	}
	config_parse_args_free(argc, argv);
	os_free(lbuf);
}
