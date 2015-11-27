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
#ifndef COAP_ESP8266_INTERFACE_H
#define COAP_ESP8266_INTERFACE_H

//-----------------------------------------------------------------------
// Please use at least ESP8266_NONOS_SDK_v1.4.1_15_10_27 !!!!
// v1.4.0 -> 1.4.1 : http://bbs.espressif.com/viewtopic.php?f=46&t=1268
//-----------------------------------------------------------------------

typedef struct {

	bool TxSocketIdle;
	uint8_t StationConStatus;

}CoAP_ESP8266_States_t;

extern CoAP_ESP8266_States_t CoAP_ESP8266_States;

bool CoAP_ESP8266_SendDatagram(uint8_t ifID, NetPacket_t* pckt);
NetSocket_t* CoAP_ESP8266_CreateInterfaceSocket(uint8_t ifID, struct espconn* pEsp8266_conn, uint16_t LocalPort, NetReceiveCallback_fn Callback, NetTransmit_fn SendPacket);
bool  CoAP_ESP8266_ConfigDevice();

#define USE_HARDCODED_CREDENTIALS (0) //other option: set via coap on soft-ap interface
#define EXTERNAL_AP_SSID "YOUR-SSID"
#define EXTERNAL_AP_PW "YOUR-WIFI-SECRET"

//enable always the softap even if connected to external router/ap
//soft ap config: ssid = "Lobaro-CoAP (ESP8266)", pw= "lobaro!!", AP-IP: 192.168.4.1
//note: soft-ap will not work properly until esp8266 is trying to connect to external router/ap
//see here why: http://bbs.espressif.com/viewtopic.php?t=324
#define SOFTAP_ALLWAYS_ON (0) //0=only soft-ap is connection to ap failed 1=soft-ap allways on

#define MAX_CON_RETRIES_BEFORE_ACTIVATING_SOFT_AP (2) //turn on the softAP after x-connection retries to external AP/Router


#endif
