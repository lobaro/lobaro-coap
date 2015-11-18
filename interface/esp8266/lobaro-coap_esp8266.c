/*******************************************************************************
 * Copyright (c)  2015  Dipl.-Ing. Tobias Rohde, http://www.lobaro.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/
/*
 * lobaro-coap-interface.c
 *
 *  Created on: 13.11.2015
 *      Author: Tobias
 */

#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_interface.h>

#include "../../coap.h"
#include "lobaro-coap_esp8266.h"

extern int ets_uart_printf(const char *fmt, ...);

//implement internaly used functions (see also lobaro-coap/interface/coap_interface.h)
void hal_uart_puts(char *s) {
	ets_uart_printf("%s",s);
}
void hal_uart_putc(char c){
	ets_uart_printf("%c",c);
}

//1Hz Clock used by timeout logic
uint32_t hal_rtc_1Hz_Cnt(void){
 return system_get_time()/1000000; //overun every ~hour, todo: use rtc timer of esp?
}

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
uint8_t* hal_nonVolatile_GetBufPtr(){
	return NULL; //not implemented yet on esp8266
}

bool hal_nonVolatile_WriteBuf(uint8_t* data, uint32_t len){
	return false; //not implemented yet on esp8266
}

bool CoAPSocket_TxOngoing = false;


//---------------------------------

static void udp_sent_cb(void *arg) {
	// struct espconn *pesp_conn = arg;

	 CoAPSocket_TxOngoing = false;
	 ets_uart_printf("send OK!\r\n");
}

//implement network functions around coap if socket
static void udp_recv_cb(void *arg, char *pdata, unsigned short len) {
	 struct espconn *pesp_conn = (struct espconn *)arg;

	 remot_info* rinfo = NULL;

	if(espconn_get_connection_info(pesp_conn, &rinfo , 0)!=0){
		ets_uart_printf("ERROR espconn_get_connection_info(...)\r\n");
	}
	ets_uart_printf("Received %d Bytes from %d.%d.%d.%d:%d  %d\r\n", len, rinfo->remote_ip[0], rinfo->remote_ip[1], rinfo->remote_ip[2], rinfo->remote_ip[3], rinfo->remote_port, rinfo->state);

	int i;
	for(i=0; i<len;i++) {
		ets_uart_printf("%x [%c] ", pdata[i] );
	}
	ets_uart_printf("\r\n");

	NetPacket_t		Packet;
	NetSocket_t* 	pSocket=NULL;

	SocketHandle_t handle = (SocketHandle_t) pesp_conn;

//get socket by handle
	pSocket = RetrieveSocket(handle);

	if(pSocket == NULL){
		ERROR("Corresponding Socket not found!\r\n");
		return;
	}

//packet data
	Packet.pData = pdata;
	Packet.size = len;

//Sender

	Packet.Sender.NetPort = rinfo->remote_port;
	Packet.Sender.NetType = IPV4;
	for(i=0; i<4; i++)
		Packet.Sender.NetAddr.IPv4.u8[i] = rinfo->remote_ip[i];

//Receiver
	Packet.Receiver.NetPort = pSocket->EpLocal.NetPort;
	Packet.Receiver.NetType = IPV4;

	for(i=0; i<4; i++)
		Packet.Receiver.NetAddr.IPv4.u8[i] = rinfo->remote_ip[i];

	for(i=0; i<4; i++)
				Packet.Receiver.NetAddr.IPv4.u8[i] = pesp_conn->proto.udp->local_ip[i];

//meta info
	Packet.MetaInfo.Type = META_INFO_NONE;

	//call the consumer of this socket
	//the packet is only valid during runtime of consuming function!
	//-> so it has to copy relevant data if needed
	// or parse it to a higher level and store this result!
	pSocket->RxCB(pSocket->ifID, &Packet);

	return;
}

NetSocket_t* ICACHE_FLASH_ATTR CoAP_ESP8266_CreateInterfaceSocket(uint8_t ifID, struct espconn* pEsp8266_conn, uint16_t LocalPort, NetReceiveCallback_fn Callback, NetTransmit_fn SendPacket)
{
	NetSocket_t* pSocket;

	if(pEsp8266_conn == NULL){
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): pEsp8266_conn can't be NULL!\r\n");
		return NULL;
	}

	pSocket=RetrieveSocket2(ifID);
	if(pSocket != NULL) {
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): interface ID already in use!\r\n");
		return NULL;
	}

	if(Callback == NULL || SendPacket == NULL) {
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): packet rx & tx functions must be provided!\r\n");
		return NULL;
	}

	pSocket = AllocSocket();
	if(pSocket == NULL){
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): failed socket allocation\r\n");
		return NULL;
	}

//prepare dummy broadcast ipv4
	char broadcastip[16];
	os_sprintf(broadcastip, "%s", "192.168.255.255");
	uint32_t ip = ipaddr_addr(broadcastip);

//local side of socket
	struct ip_info LocalIpInfo;
	pSocket->EpLocal.NetType = IPV4;
	pSocket->EpLocal.NetPort = LocalPort;
	pEsp8266_conn->proto.udp->local_port = LocalPort;

//remote side of socket
	pSocket->EpRemote.NetType = IPV4;
	pSocket->EpRemote.NetPort = LocalPort; 		//varies with requester, not known yet
	pSocket->EpRemote.NetAddr.IPv4.u32[0] = ip;

	pEsp8266_conn->proto.udp->remote_port = LocalPort; //use same for remote port here for now
	os_memcpy(pEsp8266_conn->proto.udp->remote_ip, &ip, 4);

//assign socket identification IDs (handle=internal, ifID=external)
	//create ESP8266 udp connection
	pEsp8266_conn->type = ESPCONN_UDP;
	pEsp8266_conn->state = ESPCONN_NONE;

	espconn_regist_recvcb(pEsp8266_conn, udp_recv_cb); // register a udp packet receiving callback
	espconn_regist_sentcb(pEsp8266_conn, udp_sent_cb );

	if(espconn_create(pEsp8266_conn)) {
		ets_uart_printf("CoAP_ESP8266_CreateInterfaceSocket(): internal create error\r\n");
		return NULL;
	}

	pSocket->Handle = (void*) (pEsp8266_conn); //external  to CoAP Stack
	pSocket->ifID = ifID; //internal  to CoAP Stack

//user callback registration
	pSocket->RxCB = Callback;
	pSocket->Tx = SendPacket;
	pSocket->Alive = true;

	INFO("- CoAP_ESP8266_CreateInterfaceSocket(): listening... IfID: %d  Port: %d\r\n",ifID, LocalPort);
	return pSocket;
}


static bool ICACHE_FLASH_ATTR ESP8266_Config_SoftAP(){
	struct softap_config config;
	struct ip_info info;

	wifi_softap_dhcps_stop();
	IP4_ADDR(&info.ip, 192, 168, 4, 1);
	IP4_ADDR(&info.gw, 192, 168, 4, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &info);
	wifi_softap_dhcps_start();

	config.authmode = AUTH_WPA_WPA2_PSK;
	config.beacon_interval = 100;
	config.channel = 1;
	config.max_connection = 4;
	os_memset(config.ssid, 0, 32);
	os_memset(config.password, 0, 64);
	os_memcpy(config.ssid, "Lobaro-CoAP (ESP8266)", 21);
	os_memcpy(config.password, "lobaro!!", 8); //min. length 8!
	config.ssid_hidden = 0;
	config.ssid_len = 21;
	if(wifi_softap_set_config(&config)){
		ets_uart_printf("ESP8266_Config_SoftAP(): OK\r\n");
		return true;
	}
	else {
		ets_uart_printf("ESP8266_Config_SoftAP(): ERROR\r\n");
		return false;
	}
}

#if USE_HARDCODED_CREDENTIALS == 1
static bool ICACHE_FLASH_ATTR ESP8266_Config_Station(void)
{
	char ssid[32] = EXTERNAL_AP_SSID;
	char password[64] = EXTERNAL_AP_PW;
	struct station_config config;

	config.bssid_set = 0; //need not check MAC address of AP
	os_memcpy(&config.ssid, ssid, 32);
	os_memcpy(&config.password, password, 64);

	if(wifi_station_set_config(&config)){
		ets_uart_printf("- ESP8266: SoftAP init: OK\r\n");
		return true;
	}
	else {
		ets_uart_printf("- ESP8266: SoftAP init: ERROR\r\n");
		return false;
	}
}
#endif

bool ICACHE_FLASH_ATTR CoAP_ESP8266_ConfigDevice(){
#if USE_SOFT_AP == 1
	wifi_set_opmode(STATIONAP_MODE);
	ESP8266_Config_SoftAP();

#else
	wifi_set_opmode(STATION_MODE);
#endif

#if USE_HARDCODED_CREDENTIALS == 1
	ESP8266_Config_Station();
#endif
	return true;
}

bool  ICACHE_FLASH_ATTR CoAP_ESP8266_DeleteInterfaceSocket(uint8_t ifID)
{
	NetSocket_t* pSocket = RetrieveSocket2(ifID);

	if(pSocket)
	{
		struct espconn *conn= (struct espconn *)pSocket->Handle;
		espconn_delete(conn);
		INFO("CoAP_ESP8266_DeleteInterfaceSocket(): unlisten OK! IfID: %d\r\n", ifID);
		return true;
	}

	ERROR("CoAP_ESP8266_DeleteInterfaceSocket(): Socket not found! IfID: %d\r\n", ifID);
	return false;
}

bool  ICACHE_FLASH_ATTR CoAP_ESP8266_SendDatagram(uint8_t ifID, NetPacket_t* pckt)
{
	if(pckt->Receiver.NetType != IPV4){
		ERROR("CoAP_ESP8266_SendDatagram(...): Wrong NetType!\r\n");
		return false;
	}

	NetSocket_t* pSocket;
	pSocket=RetrieveSocket2(ifID);

	if(pSocket == NULL) {
		ERROR("CoAP_ESP8266_SendDatagram(...): InterfaceID not found!\r\n");
		return false;
	}

	struct espconn* pEspConn = (struct espconn*) pSocket->Handle;

	pEspConn->proto.udp->remote_port = pckt->Receiver.NetPort;
	int i;
	for(i=0; i<4;i++) {
		pEspConn->proto.udp->remote_ip[i] = pckt->Receiver.NetAddr.IPv4.u8[i];
	}

	pEspConn->proto.udp->local_port = pckt->Sender.NetPort;

	CoAPSocket_TxOngoing = true;
	if(espconn_sendto(pEspConn, pckt->pData, pckt->size)==0){
		//INFO("ESP8266_SendDatagram(...): Send succesfully!\r\n");
		os_delay_us(1000);
		return true;
	}
	else {
			CoAPSocket_TxOngoing = false;
			ERROR("CoAP_ESP8266_SendDatagram(...): Internal Socket Error\r\n");
			return false;
	}
}


