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

#ifndef LIBLOBARO_COAP_H
#define LIBLOBARO_COAP_H

#include <stdint.h>
#include <stdbool.h>

//################################
// Endpoints
//################################

//IPv6 address
typedef union {
	uint8_t u8[16];
	uint16_t u16[8];
	uint32_t u32[4];
} NetAddr_IPv6_t;

//IPv4 address
typedef union {
	uint8_t u8[4];
	uint16_t u16[2];
	uint32_t u32[1];
} NetAddr_IPv4_t;

//UART address
typedef struct {
	uint8_t ComPortID;
} NetAddr_Uart_t;

//general address
#define NetAddr_MAX_LENGTH (16)
typedef union {
	NetAddr_IPv6_t IPv6;
	NetAddr_IPv4_t IPv4;
	NetAddr_Uart_t Uart;
	uint8_t mem[NetAddr_MAX_LENGTH]; //used for init and comparisons of addresses
} NetAddr_t;

typedef enum {
	EP_NONE, IPV6, IPV4, BTLE, UART
} NetInterfaceType_t;

// general network endpoint
// used by drivers and network libs
typedef struct {
	NetInterfaceType_t NetType;
	NetAddr_t NetAddr;
	uint16_t NetPort;
} NetEp_t;

//################################
// Packets
//################################

typedef enum {
	META_INFO_NONE,
	META_INFO_RF_PATH
} MetaInfoType_t;

typedef struct {
	uint8_t HopCount;
	int32_t RSSI;
} MetaInfo_RfPath_t;

typedef union {
	MetaInfo_RfPath_t RfPath;
} MetaInfoUnion_t;

typedef struct {
	MetaInfoType_t Type;
	MetaInfoUnion_t Dat;
} MetaInfo_t;

// general network packet
// received in callbacks and sendout
// in network send routines
typedef struct {
	uint8_t* pData;
	uint16_t size;
	// The remote EndPoint is either the sender for incoming packets or the receiver for outgoing packets
	NetEp_t remoteEp;
	// Optional meta info that will be translated into in options
	MetaInfo_t metaInfo;
} NetPacket_t;

//################################
// Sockets
//################################
typedef void* SocketHandle_t;

typedef void ( * NetReceiveCallback_fn )(SocketHandle_t socketHandle, NetPacket_t* pckt);
typedef bool ( * NetTransmit_fn )(SocketHandle_t socketHandle, NetPacket_t* pckt);


typedef struct {
	SocketHandle_t Handle; // Handle to identify the socket

	NetEp_t EpRemote;
	NetTransmit_fn Tx;     // ext. function called by coap stack to send data after finding socket by socketHandle (internally)
	bool Alive;            // We can only deal with sockets that are alive
} CoAP_Socket_t;

//################################
// Initialization
//################################

/*
 * Configuration for the CoAP stack. All fields need to be initialized.
 * Fields marked as optional can be initialized with 0
 */
typedef struct {
	// Pointer to the memory that will be used by the CoAP Stack
	uint8_t* Memory;
	int16_t MemorySize;
} CoAP_Config_t;

/*
 * API functions used by the CoAP stack. All fields need to be initialized.
 *
 * Fields marked as optional can be initialized with NULL
 */
typedef struct {
	//1Hz Clock used by timeout logic
	uint32_t (* rtc1HzCnt)(void);
	//Uart/Display function to print debug/status messages
	void (* debugPuts)(char* s);
	void (* debugPutc)(char c);
} CoAP_API_t;

//################################
// Public API
//################################

/**
 * Initialize the CoAP stack with a set of API functions used by the stack and a config struct.
 * @param api Struct with API functions that need to be defined for the stack to work
 * @param cfg Configuration values to setup the stack
 * @return A result code
 */
void CoAP_Init(CoAP_API_t api, CoAP_Config_t cfg);

/**
 * Each CoAP implementation
 * @param handle
 * @return
 */
CoAP_Socket_t* CoAP_NewSocket(SocketHandle_t handle);

//#####################
// Receive of packets
//#####################
// This function must be called by network drivers
// on reception of a new network packets which
// should be passed to the CoAP stack.
// "socketHandle" can be chosen arbitrary by calling network driver,
// but can be considered constant over runtime.
void CoAP_HandleIncomingPacket(SocketHandle_t socketHandle, NetPacket_t* pPacket);

void CoAP_doWork();

#endif //LIBLOBARO_COAP_H
