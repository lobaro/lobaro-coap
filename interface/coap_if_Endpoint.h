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
#ifndef COM_NET_EP_H
#define COM_NET_EP_H

#define  IPv6_IP(A3, A2, A1, A0)   \
		{ (A3 >> 24 & 0xff), (A3 >> 16 & 0xff), (A3 >> 8 & 0xff), (A3 >> 0 & 0xff), \
		  (A2 >> 24 & 0xff), (A2 >> 16 & 0xff), (A2 >> 8 & 0xff), (A2 >> 0 & 0xff), \
		  (A1 >> 24 & 0xff), (A1 >> 16 & 0xff), (A1 >> 8 & 0xff), (A1 >> 0 & 0xff), \
		  (A0 >> 24 & 0xff), (A0 >> 16 & 0xff), (A0 >> 8 & 0xff), (A0 >> 0 & 0xff)}

//IPv6 address
typedef union {
  uint8_t    u8 [ 16 ];
  uint16_t   u16 [ 8 ];
  uint32_t   u32 [ 4 ];
}  NetAddr_IPv6_t;

//IPv4 address
typedef union {
  uint8_t    u8 [ 4 ];
  uint16_t   u16 [ 2 ];
  uint32_t   u32 [ 1 ];
} NetAddr_IPv4_t;

//UART address
typedef struct {
  uint8_t    ComPortID;
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
	IPV6, IPV4, BTLE, UART
}NetInterfaceType_t;

//################################
// general network endpoint
// used by drivers and network libs
//################################
typedef struct
{
	NetInterfaceType_t NetType;
	NetAddr_t NetAddr;
	uint16_t  NetPort;
}NetEp_t;
//################################

extern const NetAddr_IPv6_t NetAddr_IPv6_unspecified;
extern const NetAddr_IPv6_t NetAddr_IPv6_mulitcast;

bool EpAreEqual(NetEp_t* ep_A, NetEp_t* ep_B);
void PrintEndpoint(NetEp_t* ep);

void CopyEndpoints(NetEp_t* Source, NetEp_t* Destination);

#endif
