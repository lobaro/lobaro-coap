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

#include "liblobaro_coap.h"
#include "coap.h"
#include "coap_main.h"

void debugPuts_Empty(const char* s) {
	(void) s;  // unused
}

void debugArray_Empty(const char *s, const uint8_t *array, size_t size) {
    (void) s;  // unused
    (void) array;
    (void) size;;
}

void CoAP_Init(CoAP_API_t api) {
	CoAP.api = api;

	CoAP_InitIds();

	// To make the tests stable, we should provide proper log functions in future
	if (CoAP.api.debugPuts == NULL) {
		CoAP.api.debugPuts = debugPuts_Empty;
	}

    if (CoAP.api.debugArray == NULL) {
        CoAP.api.debugArray = debugArray_Empty;
    }

#if DEBUG_RANDOM_DROP_INCOMING_PERCENTAGE != 0
	INFO("\n\nWARNING!!!\n\n    DEBUG FEATURE, DROPPING %d%% INCOMING MESSAGES ON PURPOSE!\n\n", DEBUG_RANDOM_DROP_INCOMING_PERCENTAGE);
#endif
#if DEBUG_RANDOM_DROP_OUTGOING_PERCENTAGE != 0
	INFO("\n\nWARNING!!!\n\n    DEBUG FEATURE, DROPPING %d%% OUTGOING MESSAGES ON PURPOSE!\n\n", DEBUG_RANDOM_DROP_OUTGOING_PERCENTAGE);
#endif

	INFO("CoAP_init!\r\n");
	INFO("CoAP Interaction size: %zu byte\r\n", sizeof(CoAP_Interaction_t));
	INFO("CoAP_Res_t size: %zu byte\r\n", sizeof(CoAP_Res_t));
	INFO("CoAP_Message_t size: %zu byte\r\n", sizeof(CoAP_Message_t));
	INFO("CoAP_option_t size: %zu byte\r\n", sizeof(CoAP_option_t));
	INFO("CoAP_Observer_t size: %zu byte\r\n", sizeof(CoAP_Observer_t));

	CoAP_InitResources();
}

CoAP_Result_t CoAP_Uninit(void) {
    CoAP_Result_t retval = COAP_ERR_NOT_FOUND;

    /* Unlink all interactions. */
    while (1) {
        CoAP_Interaction_t *interaction = CoAP_GetLongestPendingInteraction();
        if (interaction) {
            CoAP_DeleteInteraction(interaction);
        } else {
            break;
        }
    }

    retval = CoAP_UninitResources();
    if (COAP_OK != retval) {
        return retval;
    }

    /* Clear pinned API functions. */
    memset(&CoAP.api, 0, sizeof(CoAP_API_t));

    return retval;
}
