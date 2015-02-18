/*
 * File	: user_main.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.	If not, see <http://www.gnu.org/licenses/>.
 */
#include "espmissingincludes.h"
#include <user_interface.h>
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"

#include "driver/uart.h"
#include "task.h"

#include "server.h"
#include "config.h"
#include "flash_param.h"
#include <gpio.h>

os_event_t recvTaskQueue[recvTaskQueueLen];
extern serverConnData connData[MAX_CONN];

#define UART_BUF_SIZE 100
static char uart_buf[UART_BUF_SIZE];
static int buf_pos = 0;

#define TICK_TIME 100
#define TICK_TIMEOUT 10000
#define TICK_MAX (TICK_TIMEOUT / TICK_TIME)
#define SYNC_PAUSE 500
#define SYNC_STEP (SYNC_PAUSE / TICK_TIME)

static void add_char(char c)
{
	uart_buf[buf_pos] = c;
	buf_pos++;
	if(buf_pos >= sizeof(uart_buf)) {
		buf_pos = sizeof(uart_buf) - 1;
		// forget the oldest
		memmove(uart_buf, uart_buf+1, sizeof(uart_buf) - 1);
	}
}

static char get_char()
{
	char rs = -1;
	if(buf_pos > 0) {
		rs =  uart_buf[0];
		memmove(uart_buf, uart_buf+1, sizeof(uart_buf) - 1);
		buf_pos--;
	}
	os_printf("get char : %2X\n", rs);
	return rs;
}

static int count_chars()
{
	return buf_pos;
}

static void clean_chars()
{
	buf_pos = 0;
}

static void ICACHE_FLASH_ATTR recvTask(os_event_t *events)
{
	uint8_t temp;

	//add transparent determine
	while(READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
	{
		WRITE_PERI_REG(0X60000914, 0x73); //WTD

		temp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
		add_char(temp);
		os_printf("char=%2X\n", temp);
	}
	if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
	} else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST)) {
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
	}
	ETS_UART_INTR_ENABLE();
}






/*static void ICACHE_FLASH_ATTR recvTask(os_event_t *events)
{
	uint8_t c, i;

	uart1_sendStr("\r\nrecTask called\r\n");

	while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
	{
		WRITE_PERI_REG(0X60000914, 0x73); //WTD
		c = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

		os_printf("\r\n");
		for (i = 0; i < MAX_CONN; ++i) {
			if (connData[i].conn) {
				os_printf("<< 0x%X\r\n", c);
				char buf[10];
				os_sprintf(buf, "%c ", c);
				espconn_sent(connData[i].conn, &buf, 2);
			}
		}
//	echo
//	uart_tx_one_char(c);
	}

	if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST))
	{
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
	}
	else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_TOUT_INT_ST))
	{
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
	}
	ETS_UART_INTR_ENABLE();
}*/

void ds_init()
{
	gpio_init();

  //Set GPIO2 to output mode
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
//  PIN_FUNC_SELECT(PERIPHS_IO_MUX_XPD_DCDC_U, FUNC_GPIO16);




	//set gpio5 as gpio pin
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	//disable pulldown
	PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO5_U);

	//enable pull up R
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);

}

static ETSTimer delayTimer;


static void ICACHE_FLASH_ATTR xProcess(void *arg)
{
	char buf[64];
	int n = count_chars(), i, j;
	if(n > 0) {
		os_printf("n=%d\n", n);
		if(n > sizeof(buf)) {
			n = sizeof(buf);
		}
		for (i = 0; i < MAX_CONN; ++i) {
			if (connData[i].conn) {
				for(j=0; j<n; j++) {
					buf[j] = get_char();
					os_printf("[%2X] ", buf[j]);
				}
				espconn_sent(connData[i].conn, (uint8_t*)buf, n);
				os_printf("\nSent %d bytes\n", n);
			}
		}
	}
}

void user_init(void)
{
//	system_set_os_print(1);
	flash_param_t *flash_param = flash_param_get();
	uart_init(flash_param->baud, BIT_RATE_115200);
//	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000);
	os_printf("Serial baud rate: %d\n", flash_param->baud);
	ds_init();
//	GPIO_OUTPUT_SET(5, 1);
//	os_delay_us(1000);
//	GPIO_OUTPUT_SET(5, 0);
//	os_delay_us(1000);
//	GPIO_OUTPUT_SET(5, 1);
//	os_printf("Starting...\n");

	// refresh wifi config
	config_execute(flash_param);

	serverInit(flash_param->port);

	system_os_task(recvTask, recvTaskPrio, recvTaskQueue, recvTaskQueueLen);
	os_timer_disarm(&delayTimer);
	os_timer_setfn(&delayTimer, xProcess, NULL);
	os_timer_arm(&delayTimer, 100, 1);
}
