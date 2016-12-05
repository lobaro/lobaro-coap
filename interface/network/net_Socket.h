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
#ifndef COM_NET_SOCKET_H
#define COM_NET_SOCKET_H

#include <stdbool.h>
#include <stdint.h>

//function pointer typedefs, used below in struct
typedef void *SocketHandle_t;

typedef void ( *NetReceiveCallback_fn )(SocketHandle_t socketHandle, NetPacket_t *pckt);

typedef bool ( *NetTransmit_fn )(SocketHandle_t socketHandle, NetPacket_t *pckt);


typedef struct {
	SocketHandle_t Handle;  // Handle to identify the socket

	NetEp_t EpLocal;
	NetEp_t EpRemote;
	NetReceiveCallback_fn RxCB; //callback function on receiving data (normally set to "CoAP_onNewPacketHandler")
	NetTransmit_fn Tx;            //ext. function called by coap stack to send data after finding socket by socketHandle (internally)
	bool Alive;
} CoAP_Socket_t;

#define MAX_ACTIVE_SOCKETS (5)

CoAP_Socket_t *AllocSocket();

CoAP_Socket_t *RetrieveSocket(SocketHandle_t handle);

#endif
