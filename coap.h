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
 * coap.h
 *
 *  Created on: 05.11.2014
 *      Author: Tobias
 */

#ifndef COAP_H_
#define COAP_H_

//Standard c-libs
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//Interface "glue" to surrounding project/software
//to use the stack you have to provide some packet send/receive functionality
//it's up to you which packet format you use e.g. UDP is the most obvious...
//-> see coap_main.h for more information and adding send/receive interface functions
#include "interface/coap_if_Endpoint.h"
#include "interface/coap_if_Packet.h"
#include "interface/coap_if_Socket.h"

//Internal stack functions
typedef enum
{
	COAP_OK=0,
	COAP_NOT_FOUND, //not found but no error
	COAP_PARSE_DATAGRAM_TOO_SHORT,
	COAP_PARSE_UNKOWN_COAP_VERSION,
	COAP_PARSE_MESSAGE_FORMAT_ERROR,
	COAP_PARSE_TOO_MANY_OPTIONS,
	COAP_PARSE_TOO_LONG_OPTION,
	COAP_PARSE_TOO_MUCH_PAYLOAD,
	COAP_PACK_TOO_MANY_OPTIONS,
	COAP_PACK_TOO_LONG_OPTION,
	COAP_ERR_ARGUMENT,
	COAP_ERR_SOCKET,
	COAP_ERR_NETWORK,
	COAP_ERR_OUT_OF_MEMORY,
	COAP_ERR_TOO_LONG_URI_PATH,
	COAP_ERR_NOT_FOUND,
	COAP_ERR_WRONG_OPTION,
	COAP_ERR_EXISTING,
	COAP_TRUE,
	COAP_FALSE,
	COAP_ERR_WRONG_REQUEST,
	COAP_BAD_OPTION_VAL,
	COAP_BAD_OPTION_LEN,
	COAP_REMOVED,
	COAP_ERR_UNKNOWN,
	COAP_ERR_REMOTE_RST,
	COAP_ERR_OUT_OF_ATTEMPTS,
	COAP_ERR_TIMEOUT,
	COAP_WAITING,
	COAP_HOLDING_BACK,
	COAP_RETRY
}CoAP_Result_t;

#include "coap_debug.h"

#define COAP_RAM_TOTAL_BYTES (4096)
#include "coap_mem.h"

#include "coap_options.h"
#include "coap_message.h"
#include "option-types/coap_option_blockwise.h"
#include "option-types/coap_option_ETag.h"
#include "option-types/coap_option_cf.h"
#include "option-types/coap_option_uri.h"
#include "option-types/coap_option_observe.h"
#include "coap_observe.h"
#include "coap_resource.h"
#include "coap_interaction.h"
#include "coap_main.h"

typedef struct {
	CoAP_Interaction_t* pInteractions;
}CoAP_t;

extern CoAP_t CoAP; //Stack global variables

#endif /* COAP_H_ */
