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

CoAP_ESP8266_States_t CoAP_ESP8266_States = {.TxSocketIdle=true, .StationConStatus=0xff};

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

//---------------------------------
static void udp_sent_cb(void *arg) {
	// struct espconn *pesp_conn = arg;
 CoAP_ESP8266_States.TxSocketIdle = true;
 ets_uart_printf("send OK!\r\n");
}

//implement network functions around coap if socket
static void udp_recv_cb(void *arg, char *pdata, unsigned short len) {
	 struct espconn *pesp_conn = (struct espconn *)arg;

	 remot_info* rinfo = NULL;

	if(espconn_get_connection_info(pesp_conn, &rinfo , 0)!=0){
		ets_uart_printf("ERROR espconn_get_connection_info(...)\r\n");
	}
	ets_uart_printf("Received %d Bytes from %d.%d.%d.%d:%d\r\n", len, rinfo->remote_ip[0], rinfo->remote_ip[1], rinfo->remote_ip[2], rinfo->remote_ip[3], rinfo->remote_port);

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

	CoAP_ESP8266_States.TxSocketIdle = false;
	if(espconn_sendto(pEspConn, pckt->pData, pckt->size)==0){
		//INFO("ESP8266_SendDatagram(...): Send succesfully!\r\n");
		return true;
	}
	else {
		CoAP_ESP8266_States.TxSocketIdle = true;
			ERROR("CoAP_ESP8266_SendDatagram(...): Internal Socket Error\r\n");
			return false;
	}
}


//########################### Wifi connection management ##############################
LOCAL os_timer_t Connection_timer;

static bool ICACHE_FLASH_ATTR ESP8266_Config_SoftAP(){
	static struct softap_config config;
	struct ip_info info;
	char macaddr[6];

	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_get_config(&config);
	wifi_softap_reset_dhcps_lease_time();



	wifi_get_macaddr(SOFTAP_IF, macaddr);
	os_memset((void*)config.ssid, 0, sizeof(config.ssid));
	os_sprintf(config.ssid, "Lobaro-CoAP(%02x%02x%02x%02x%02x%02x)", MAC2STR(macaddr));
	config.ssid_len = 0;
	os_memset((void*)config.password, 0, sizeof(config.password));
	os_memcpy(config.password,"lobaro!!",8);

	config.ssid_hidden = 0;
	config.authmode = AUTH_WPA2_PSK;//AUTH_WPA_WPA2_PSK; //AUTH_WPA_WPA2_PSK;
	config.beacon_interval=250;
	config.channel=1;
	config.max_connection = 4; // how many stations can connect to ESP8266 softAP at most.

	wifi_softap_set_config_current(&config);// Set ESP8266 softap config

	wifi_softap_dhcps_stop();
	IP4_ADDR(&info.ip, 192, 168, 4, 1);
	IP4_ADDR(&info.gw, 192, 168, 4, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &info);
	wifi_softap_dhcps_start();

	struct ip_info ipconfig;
	wifi_get_ip_info(SOFTAP_IF, &ipconfig);
	ets_uart_printf("- SoftAP started at IP -> %d.%d.%d.%d pw=%s ssid=%s\r\n",IP2STR(&ipconfig.ip), config.password,config.ssid);

	return true;

}

static bool  ICACHE_FLASH_ATTR ESP8266_Config_Station(void)
{
	struct station_config config;

#if USE_HARDCODED_CREDENTIALS == 1
	char ssid[32] = EXTERNAL_AP_SSID;
	char password[64] = EXTERNAL_AP_PW;
	config.bssid_set = 0; //need not check MAC address of AP
	os_memcpy(&config.ssid, ssid, 32);
	os_memcpy(&config.password, password, 64);
#else
	if(wifi_station_get_config_default(&config)==false) {
		ets_uart_printf("- ESP8266: No default wifi config present!\r\n");
		return false;
	}
#endif

	if(wifi_station_set_config(&config)){
		ets_uart_printf("- Connecting to as station to external WIFI AP/Router [ssid=\"%s\"] \r\n", config.ssid);
		wifi_set_opmode(STATION_MODE);
		wifi_station_connect();
		return true;
	}
	else {
		ets_uart_printf("- (!!!!) Could not set station config!\r\n");
		return false;
	}
}

LOCAL void ICACHE_FLASH_ATTR connection_timer_cb(void *arg) {
    static uint8_t failRetryCnt=0;

	uint8_t conStatus = wifi_station_get_connect_status();

	if (conStatus == STATION_GOT_IP) { //just got ip
		if(conStatus == CoAP_ESP8266_States.StationConStatus) {
			return; //connected
		} else { //just got connected...
			failRetryCnt=0;
			ets_uart_printf("\r\n- Got IP from external AP! -> ");
			struct ip_info ipconfig;
			if(wifi_get_ip_info(STATION_IF, &ipconfig)){
				  ets_uart_printf("%d.%d.%d.%d\r\n",IP2STR(&ipconfig.ip));
			}
			else ets_uart_printf("ERROR!!\r\n");
		}
	}else if(conStatus == STATION_CONNECTING && conStatus!=CoAP_ESP8266_States.StationConStatus ){
		ets_uart_printf("...connecting to remote wifi access point...\r\n");

	}else if(conStatus == STATION_WRONG_PASSWORD && conStatus!=CoAP_ESP8266_States.StationConStatus ){
		ets_uart_printf("(!!!) Wrong Password!\r\n");
		failRetryCnt++;
		wifi_station_connect();

	}else if(conStatus == STATION_NO_AP_FOUND && conStatus!=CoAP_ESP8266_States.StationConStatus ){
		failRetryCnt++;
		ets_uart_printf("(!!!) No AP Found!\r\n");
		wifi_station_connect();

	}else if(conStatus == STATION_CONNECT_FAIL && conStatus!=CoAP_ESP8266_States.StationConStatus ){
		failRetryCnt++;
		ets_uart_printf("(!!!) Connect Fail\r\n");
		wifi_station_connect();
	}
//	}else if(conStatus == STATION_IDLE && conStatus!=CoAP_ESP8266_States.StationConStatus ){
//		failRetryCnt++;
//		ets_uart_printf("(!!!) Idle, maybe no SSID/PW configured?\r\n");
//	}

	CoAP_ESP8266_States.StationConStatus = conStatus;

	if(failRetryCnt > MAX_CON_RETRIES_BEFORE_ACTIVATING_SOFT_AP) {
#if SOFTAP_ALLWAYS_ON == 0
		ets_uart_printf("(!!!) giving up to connect!\r\nTry config via softAP interface (coap://192.168.4.1:5683) and reset esp8266\r\n");
		wifi_station_disconnect(); //don't retry automatically to let soft-ap work properly (which isn't functional during station connecting!)
		ESP8266_Config_SoftAP(); //enable soft-ap mode
#else
		wifi_station_disconnect(); //don't retry automatically to let soft-ap work properly (which isn't functional during station connecting!)
#endif
		failRetryCnt = 0;
	}
}

bool ICACHE_FLASH_ATTR CoAP_ESP8266_ConfigDevice(){

	struct station_config cfg;

	wifi_set_phy_mode(PHY_MODE_11G);

#if SOFTAP_ALLWAYS_ON == 1
	ESP8266_Config_SoftAP(); //enables STATION+SOFTAP mode
#else
	wifi_set_opmode(STATION_MODE);
#endif
	wifi_station_get_config(&cfg);

	//if no config found (e.g. 1st start after flash clear) -> write some valid but dummy config to get statemachine working
	if(coap_strlen(cfg.ssid)==0) {
		wifi_station_disconnect();

		os_memset(cfg.ssid, 0, 32);
		os_memset(cfg.password, 0, 64);
		os_memset(cfg.bssid, 0, 6);
		cfg.bssid_set = 0;

		ets_uart_printf("ssid not configured!\r\n");
		coap_strcpy(cfg.ssid,"not_configured!");
		coap_strcpy(cfg.password,"12345678");
		wifi_station_set_config(&cfg);
	}


	if(ESP8266_Config_Station()==false) { //no valid config/could not start station connect
		ets_uart_printf("(!!!) Could not start connect to to external AP. Try config via softap interface (coap://192.168.4.1:5683) and reset esp8266\r\n");
		ESP8266_Config_SoftAP(); //enables STATION+SOFTAP mode
	}

	//start timer for monitoring wifi station connection status to external router
	os_timer_disarm(&Connection_timer);
	os_timer_setfn(&Connection_timer, (os_timer_func_t *)connection_timer_cb, (void *)0);
	os_timer_arm(&Connection_timer, 2000, 1);

	return true;
}
