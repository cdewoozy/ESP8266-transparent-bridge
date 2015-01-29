#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include <gpio.h>

#include "server.h"
#include "config.h"

static struct espconn serverConn;
static esp_tcp serverTcp;
serverConnData connData[MAX_CONN];

static bool reset_sent;

static serverConnData ICACHE_FLASH_ATTR *serverFindConnData(void *arg) {
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn==(struct espconn *)arg) return &connData[i];
	}
	//os_printf("FindConnData: Huh? Couldn't find connection for %p\n", arg);
	return NULL; //WtF?
}


static void ICACHE_FLASH_ATTR serverSentCb(void *arg) {
	int r;
	serverConnData *conn=serverFindConnData(arg);
	if (conn==NULL) return;
}

static void ICACHE_FLASH_ATTR serverRecvCb(void *arg, char *data, unsigned short len) {
	int x, i;
	char *p, *e;
	serverConnData *conn=serverFindConnData(arg);
	if (conn==NULL) return;

	if (len >= 5 && data[0] == '+' && data[1] == '+' && data[2] == '+' && data[3] =='A' && data[4] == 'T') {
		config_parse(conn->conn, data, len);
	} else {
		uart0_tx_buffer(data, len);
		for(i=0; i<len; i++) {
			os_printf(">> 0x%2X ", data[i]);
		}
		os_printf("\r\n");
		if( len == 2 && data[0] == 0x30 && data[1] == 0x20 && ! reset_sent ) {
			// send reset to arduino
	GPIO_OUTPUT_SET(5, 0);
	os_printf("Send Reset\n");
	os_delay_us(1000000L);
	GPIO_OUTPUT_SET(5, 1);
			reset_sent = true;
		}
	}
}

static void ICACHE_FLASH_ATTR serverReconCb(void *arg, sint8 err) {
	serverConnData *conn=serverFindConnData(arg);
	if (conn==NULL) return;
	//Yeah... No idea what to do here. ToDo: figure something out.
}

static void ICACHE_FLASH_ATTR serverDisconCb(void *arg) {
	//Just look at all the sockets and kill the slot if needed.
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn!=NULL) {
			if (connData[i].conn->state==ESPCONN_NONE || connData[i].conn->state==ESPCONN_CLOSE) {
				connData[i].conn=NULL;
			}
		}
	}
}

static void ICACHE_FLASH_ATTR serverConnectCb(void *arg) {
	struct espconn *conn = arg;
	int i;
	//Find empty conndata in pool
	for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
	if (i==MAX_CONN) {
		//os_printf("Aiee, conn pool overflow!\n");
		espconn_disconnect(conn);
		return;
	}
	connData[i].conn=conn;
	connData[i].buff=NULL;
	os_printf("Connected\n");
	reset_sent = false;
	espconn_regist_recvcb(conn, serverRecvCb);
	espconn_regist_reconcb(conn, serverReconCb);
	espconn_regist_disconcb(conn, serverDisconCb);
	espconn_regist_sentcb(conn, serverSentCb);
}

void ICACHE_FLASH_ATTR serverInit(int port) {
	serverConn.type=ESPCONN_TCP;
	serverConn.state=ESPCONN_NONE;
	serverTcp.local_port=port;
	serverConn.proto.tcp=&serverTcp;

	espconn_regist_connectcb(&serverConn, serverConnectCb);
	espconn_accept(&serverConn);
	espconn_regist_time(&serverConn, SERVER_TIMEOUT, 0);
}
