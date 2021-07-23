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
#include "../../coap.h"

#define LAST_SOCKET_IDX ( MAX_ACTIVE_SOCKETS - 1 )

CoAP_Socket_t SocketMemory[MAX_ACTIVE_SOCKETS] = {0} ;

CoAP_Socket_t* _rom AllocSocket() {
	int i;

	for (i = 0; i < MAX_ACTIVE_SOCKETS; i++) {
		CoAP_Socket_t* socket = &SocketMemory[i];
		if (socket->Alive == false) {
			socket->Alive = true;
			return socket;
		}
	}

	return NULL; //no free memory
}

CoAP_Result_t _rom FreeSocket(CoAP_Socket_t *socket) {
    int i;

    for (i = 0; i < MAX_ACTIVE_SOCKETS; i++) {
        if (socket == &SocketMemory[i]) {
            memset(socket, 0, sizeof(*socket));
            return COAP_OK;
        }
    }
    return COAP_ERR_NOT_FOUND;
}

CoAP_Socket_t* _rom RetrieveSocket(SocketHandle_t handle) {
	int i;
	for (i = 0; i < MAX_ACTIVE_SOCKETS; i++) {
		if (SocketMemory[i].Alive &&
			SocketMemory[i].Handle == handle) //corresponding socket found!
		{
			return &SocketMemory[i];
		}

	}
	return NULL; //not found
}
