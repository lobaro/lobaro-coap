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

#include <stdio.h>
#include <string.h>
#include "../../coap.h"

const NetAddr_IPv6_t NetAddr_IPv6_unspecified = {.u8 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }};
const NetAddr_IPv6_t NetAddr_IPv6_mulitcast = {.u8 = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }};
const NetAddr_IPv4_t NetAddr_IPv4_unspecified = { .u8 = { 0, 0, 0, 0 } };
const NetAddr_IPv4_t NetAddr_IPv4_mulitcast = { .u8 = { 224, 0, 1, 187 } };

const NetEp_t NetEp_IPv6_mulitcast = { .NetType = IPV6, .NetPort = 5683, .NetAddr = { .IPv6 = {.u8 = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } } } };
const NetEp_t NetEp_IPv4_mulitcast = { .NetType = IPV4, .NetPort = 5683, .NetAddr = { .IPv4 = { .u8 = { 224, 0, 1, 187 } } } };

 bool _rom EpAreEqual(const NetEp_t* ep_A, const NetEp_t* ep_B)
 {
	if (!ep_A || !ep_B) { return false; }
	if (ep_A->NetType != ep_B->NetType) { return false; }
	if (ep_A->NetPort != ep_B->NetPort) { return false; }

	if (ep_A->NetType == IPV6) {
		if (ep_A->NetAddr.IPv6.u32[0] != ep_B->NetAddr.IPv6.u32[0]
			|| ep_A->NetAddr.IPv6.u32[1] != ep_B->NetAddr.IPv6.u32[1]
			|| ep_A->NetAddr.IPv6.u32[2] != ep_B->NetAddr.IPv6.u32[2]
			|| ep_A->NetAddr.IPv6.u32[3] != ep_B->NetAddr.IPv6.u32[3]) { return false; }
	} else if (ep_A->NetType == IPV4) {
		if (ep_A->NetAddr.IPv4.u8[0] != ep_B->NetAddr.IPv4.u8[0]
			|| ep_A->NetAddr.IPv4.u8[1] != ep_B->NetAddr.IPv4.u8[1]
			|| ep_A->NetAddr.IPv4.u8[2] != ep_B->NetAddr.IPv4.u8[2]
			|| ep_A->NetAddr.IPv4.u8[3] != ep_B->NetAddr.IPv4.u8[3]) { return false; }
	} else {
		int i;
		for (i = 0; i < NetAddr_MAX_LENGTH; i++) {
			if (ep_A->NetAddr.mem[i] != ep_B->NetAddr.mem[i]) { return false; }
		}
	}

	return true;
}

void _rom CopyEndpoints(NetEp_t* Destination, const NetEp_t* Source) {
	memmove((void*) Destination, (void*) Source, sizeof(NetEp_t));
}

NetInterfaceType_t _rom CoAP_ParseNetAddress(NetAddr_t *addr, const char *s) {
 	if (addr == NULL) {
 		return EP_NONE;
 	}
 	// try IPv4:
 	int i0=-1, i1=-1, i2=-1, i3=-1;
	int got = sscanf(s, "%d.%d.%d.%d", &i0, &i1, &i2, &i3);
	// Log("PARSE: %d\n", got);
 	if (got==4) {
 		if (0<=i0 && i0<=255 && 0<=i1 && i1<=255 && 0<=i2 && i2<=255 && 0<=i3 && i3<=255) {
 			addr->IPv4.u8[0] = i0;
 			addr->IPv4.u8[1] = i1;
 			addr->IPv4.u8[2] = i2;
 			addr->IPv4.u8[3] = i3;
 			return IPV4;
 		}
 	}
 	// try IPv6:
 	// TODO:
 	return EP_NONE;
}
