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
#ifndef COAP_MAIN_H_
#define COAP_MAIN_H_

#define HOLDTIME_AFTER_NON_TRANSACTION_END (0)
#define POSTPONE_WAIT_TIME_SEK (3)
#define POSTPONE_MAX_WAIT_TIME (30)
#define CLIENT_MAX_RESP_WAIT_TIME (45)

#define USE_RFC7641_ADVANCED_TRANSMISSION (1) //Update representation of resource during retry of observe sendout

#define ACK_TIMEOUT (2)
#define ACK_RANDOM_FACTOR (1.5)
#define MAX_RETRANSMIT (4)
#define NSTART (1) //todo implement
#define DEFAULT_LEISURE (5) //todo implement
#define PROBING_RATE (1)        //[client]

/*
 * Configuration for the CoAP stack. All fields need to be initialized.
 * Fields marked as optional can be initialized with 0
 */
typedef struct {
	// Pointer to the memory that will be used by the CoAP Stack
	uint8_t *Memory;
	int16_t MemorySize;
} CoAP_Config_t;

/*
 * API functions used by the CoAP stack. All fields need to be initialized.
 *
 * Fields marked as optional can be initialized with NULL
 */
typedef struct {
	uint32_t (*rtc1HzCnt)(void);
	void (*debugPutc)(char c);
	void (*debugPuts)(char *s);
} CoAP_API_t;

//#####################
// Receive of packets
//#####################
// This function must be called by network drivers
// on reception of a new network packets which
// should be passed to the CoAP stack.
// "socketHandle" can be chosen arbitrary by calling network driver,
// but can be considered constant over runtime.
void CoAP_onNewPacketHandler(SocketHandle_t socketHandle, NetPacket_t *pckt);

//#####################
// Transmit of packets
//#####################

/**
 * Initialize the CoAP stack with a set of API functions used by the stack and a config struct.
 * @param api Struct with API functions that need to be defined for the stack to work
 * @param cfg Configuration values to setup the stack
 * @return A result code
 */
CoAP_Result_t CoAP_Init(CoAP_API_t api, CoAP_Config_t cfg);

/**
 * Each CoAP implementation
 * @param handle
 * @return
 */
CoAP_Socket_t *CoAP_NewSocket(void *handle);

void CoAP_doWork();

#endif
