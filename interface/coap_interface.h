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

#ifndef USER_LOBARO_COAP_INTERFACE_COAP_INTERFACE_H_
#define USER_LOBARO_COAP_INTERFACE_COAP_INTERFACE_H_

//-------------------------------------------------------------------------
// lobaro-coap needs some std C language functions/headers
//-------------------------------------------------------------------------
	#include <stdlib.h>

	#include <stdarg.h>
	#include <stddef.h>
	#include <stdio.h>
	#include <string.h>

//ESP8266 specific glue (because the sdk has no complete std c lib)
#ifdef __ets__ //to be defined ("-D__ets__") in makefile as compiler argument (normally given with ESP8266 SDK)
	//ESP8266 with partial libC from SDK
	#include <osapi.h>
	#include <os_type.h>

	#define _rom ICACHE_FLASH_ATTR
	#define _ram

	#define coap_sprintf os_sprintf
	#define coap_printf  ets_uart_printf
	#define coap_memcpy  os_memcpy
	#define coap_memset  os_memset
	#define coap_memmove os_memmove
	#define coap_strlen  os_strlen
	#define	coap_strstr  os_strstr
	#define coap_strcpy  os_strcpy
	#define coap_memcmp  os_memcmp
#else
	#include <stdint.h>
	#include <stdbool.h>
	#define coap_sprintf sprintf
	#define coap_printf printf
	#define coap_memcpy memcpy
	#define coap_memset memset
	#define coap_memmove memmove
	#define coap_strlen strlen
	#define	coap_strstr strstr
	#define coap_strcpy strcpy
	#define coap_memcmp memcmp

	#define _rom
	#define _ram
#endif

//----------------------------------------------------------------------------------
//Interface "glue" to surrounding project/software
//for use of the stack you have to provide some packet send/receive functionality
//it's up to you which packet format you use e.g. UDP is the most obvious...
//-> see coap_main.h for more information and adding send/receive interface functions
//----------------------------------------------------------------------------------

#include "_common/coap_if_Endpoint.h"
#include "_common/coap_if_Packet.h"
#include "_common/coap_if_Socket.h"

//-------------------------------------------------------------------------
//Implementation for these function prototypes must be provided externally:
//-------------------------------------------------------------------------

//Uart/Display function to print debug/status messages to
void hal_uart_puts(char *s);
void hal_uart_putc(char c);

//1Hz Clock used by timeout logic
uint32_t hal_rtc_1Hz_Cnt(void);

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
uint8_t* hal_nonVolatile_GetBufPtr();
bool hal_nonVolatile_WriteBuf(uint8_t* data, uint32_t len);

#endif
