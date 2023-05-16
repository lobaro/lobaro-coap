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

#include "coap.h"
#include <inttypes.h>

#define KNOWN_OPTIONS_COUNT     (sizeof(KNOWN_OPTIONS) / sizeof(KNOWN_OPTIONS[0]))

// Used in Critical Option Check.
uint16_t KNOWN_OPTIONS[ ] =
{
    OPT_NUM_URI_PATH,
    OPT_NUM_BLOCK2,
    OPT_NUM_BLOCK1,
    OPT_NUM_ETAG,
    OPT_NUM_CONTENT_FORMAT,
    OPT_NUM_URI_QUERY,
    OPT_NUM_ACCEPT,
    OPT_NUM_URI_PORT,
    OPT_NUM_OSCORE,
    OPT_NUM_URI_HOST
};

//#########################################################################################################
//### This function packs multiple CoAP options to the format specified at
//### section 3.1 in RFC7252. The option array gets sorted by option numbers first and
//###  is than packed into the compressed byte array format with its delta encoding.
//#########################################################################################################
CoAP_Result_t _rom pack_OptionsFromList(uint8_t* pDestArr, uint16_t* pBytesWritten, CoAP_option_t* pOptionsListBegin) {
	uint16_t offset = 0;         //Current "Write" Position while packing the options array to the byte array
	uint16_t OptStartOffset = 0; //Position of 1st Byte of current packed option
	uint16_t lastOptNumber = 0;
	uint16_t currDelta = 0;         //current Delta to privious option
	uint16_t optLength = 0;         //Length of current Option

	CoAP_option_t* pOption = pOptionsListBegin;

	if (pOption == NULL) //no options - list empty - can happen no error
	{
		*pBytesWritten = 0;
		return COAP_OK;
	}

	//iterate throw array of options
	do {
		//Inits for Option Packing
		currDelta = pOption->Number - lastOptNumber;
		lastOptNumber = pOption->Number;

		optLength = pOption->Length;
		OptStartOffset = offset;
		offset++;
		pDestArr[OptStartOffset] = 0;

		//Delta Bytes
		if (currDelta < 13) {
			pDestArr[OptStartOffset] |= ((uint8_t) currDelta) << 4u;
		} else if (currDelta < 269) {
			pDestArr[OptStartOffset] |= ((uint8_t) 13u) << 4u;
			pDestArr[offset] = ((uint8_t) currDelta) - 13u;
			offset++;
		} else {
			pDestArr[OptStartOffset] |= ((uint8_t) 14u) << 4u;

			pDestArr[offset] = (uint8_t) ((currDelta - 269u) >> 8u);
			offset++;
			pDestArr[offset] = (uint8_t) ((currDelta - 269u) & 0xffu);
			offset++;
		}

		//Length Bytes
		if (optLength < 13) {
			pDestArr[OptStartOffset] |= ((uint8_t) optLength);
		} else if (optLength < 269) {
			pDestArr[OptStartOffset] |= ((uint8_t) 13);
			pDestArr[offset] = ((uint8_t) optLength) - 13;
			offset++;
		} else {
			pDestArr[OptStartOffset] |= ((uint8_t) 14);

			pDestArr[offset] = (uint8_t) ((optLength - 269u) >> 8u);
			offset++;
			pDestArr[offset] = (uint8_t) ((optLength - 269u) & 0xffu);
			offset++;
		}

		//Option Values
		int t;
		for (t = 0; t < optLength; t++) {
			pDestArr[offset] = pOption->Value[t];
			offset++;
		}

		if (pOption->next == NULL)
			break;
		pOption = pOption->next;
	} while (1);

	*pBytesWritten = offset;
	return COAP_OK;
}

uint16_t _rom CoAP_NeededMem4PackOptions(CoAP_option_t* pOptionsListBegin) {
	uint16_t offset = 0;         //Current "Write" Position while packing the options array to the byte array
	uint16_t lastOptNumber = 0;
	uint16_t currDelta = 0;         //current Delta to privious option
	uint16_t optLength = 0;         //Length of current Option

	CoAP_option_t* pOption = pOptionsListBegin;

	if (pOption == NULL) {         //no options - list empty - can happen no error
		return 0;
	}

	//iterate throw array of options
	do {
		//Inits for Option Packing
		currDelta = pOption->Number - lastOptNumber;
		lastOptNumber = pOption->Number;

		optLength = pOption->Length;
		offset++;

		//Delta Bytes
		if (currDelta < 13) {
			;
		}
		else if (currDelta < 269)
			offset++;
		else
			offset += 2;

		//Length Bytes
		if (optLength < 13) {
			;
		}
		else if (optLength < 269)
			offset++;
		else
			offset += 2;

		//Option Values
		int t;
		for (t = 0; t < optLength; t++)
			offset++;

		if (pOption->next == NULL)
			break;
		pOption = pOption->next;
	} while (1);

	return offset;
}

CoAP_Result_t _rom parse_OptionsFromRaw(uint8_t* srcArr, uint16_t srcLength, uint8_t** pPayloadBeginInSrc, CoAP_option_t** pOptionsListBegin) {
	//srcArr points to the beginning of Option section @ raw datagram byte array
	//length includes payload marker & payload (if any)
	uint16_t offset = 0;
	*pPayloadBeginInSrc = NULL;

	if (*pOptionsListBegin != NULL) {
		INFO("- Option list argument must be an empty list!\r\n");
		return COAP_ERR_ARGUMENT;
	}

	if (srcLength == 0) {
		return COAP_OK;
	}

	uint16_t lastOptionNumber = 0; // used for delta calculations of optionnumbers

	while (offset < srcLength) {
		if (srcArr[offset] == OPTION_PAYLOAD_MARKER) // Payload Marker
		{
			if ((srcLength - offset) < 2) {
				INFO("- at least one byte payload must follow to the payload marker\r\n");
				return COAP_PARSE_MESSAGE_FORMAT_ERROR;
			}

			*pPayloadBeginInSrc = &(srcArr[offset + 1]);
			return COAP_OK;
		} else {
			uint8_t currOptDeltaField = srcArr[offset] >> 4u;
			uint16_t currOptDelta = currOptDeltaField; // init with field data, but can be overwritten if field set to 13 or 14
			uint8_t currOptLengthField = srcArr[offset] & 0x0fu;
			uint16_t currOptLength = currOptLengthField; // init with field data, but can be overwritten if field set to 13 or 14

			offset++;

			//Option Delta extended (if any)
			if (currOptDeltaField == 13) {
				// 13:  An 8-bit unsigned integer follows the initial byte and
				// indicates the Option Delta minus 13.
				currOptDelta = srcArr[offset] + 13;
				offset++;
			} else if (currOptDeltaField == 14) {
				// 14:  A 16-bit unsigned integer in network byte order follows the
				// initial byte and indicates the Option Delta minus 269.
				currOptDelta = ((((uint16_t) srcArr[offset]) << 8u) | ((uint16_t) srcArr[offset + 1])) + 269u;
				offset += 2;
			} else if (currOptDeltaField == 15) {
				// 15:  Reserved for the Payload Marker.  If the field is set to this
				// value but the entire byte is not the payload marker, this MUST
				// be processed as a message format error.
				// NOTE: we checked for OPTION_PAYLOAD_MARKER before!
				INFO("- currOptDeltaField == 15 is not allowed(1)\r\n");
				return COAP_PARSE_MESSAGE_FORMAT_ERROR;
			}

			//Option Length extended (if any)
			if (currOptLengthField == 13) {
				currOptLength = srcArr[offset] + 13;
				offset++;
			} else if (currOptLengthField == 14) {
				currOptLength = ((((uint16_t) srcArr[offset]) << 8u) | ((uint16_t) srcArr[offset + 1])) + 269u;
				offset += 2;
			} else if (currOptLengthField == 15) {
				INFO("- currOptDeltaField == 15 is not allowed %x (2)\r\n", srcArr[offset - 1]);
				return COAP_PARSE_MESSAGE_FORMAT_ERROR;
			}

			if (currOptLength > MAX_OPTION_VALUE_SIZE) {
				INFO("- Option too long\r\n");
				return COAP_PARSE_MESSAGE_FORMAT_ERROR;
			}
			if ((srcLength - offset) < currOptLength) {
				INFO("- Option too short\r\n");
				return COAP_PARSE_MESSAGE_FORMAT_ERROR;
			}

			lastOptionNumber = currOptDelta + lastOptionNumber;

			//add this option to ordered linked list
			CoAP_Result_t Res = CoAP_AppendOptionToList(pOptionsListBegin, lastOptionNumber, &(srcArr[offset]), currOptLength);
			if (Res != COAP_OK)
				return Res;

			offset += currOptLength;
		}
	}

	return COAP_OK;
}

CoAP_Result_t _rom CoAP_RemoveOptionFromList(CoAP_option_t** pOptionListStart, CoAP_option_t* pOptionToRemove) {
	CoAP_option_t* currP;
	CoAP_option_t* prevP;

	// For 1st node, indicate there is no previous.
	prevP = NULL;

	//Visit each node, maintaining a pointer to
	//the previous node we just visited.
	for (currP = *pOptionListStart; currP != NULL;
			prevP = currP, currP = currP->next) {

		if (currP == pOptionToRemove) {  // Found it.
			if (prevP == NULL) {
				//Fix beginning pointer.
				*pOptionListStart = currP->next;
			} else {
				//Fix previous node's next to
				//skip over the removed node.
				prevP->next = currP->next;
			}

			// Deallocate the node.
			CoAP.api.free((void*) currP);

			//Done searching.
			return COAP_OK;
		}
	}
	return COAP_OK;
}

static CoAP_Result_t _rom append_OptionToListEnd(CoAP_option_t** pOptionsListBegin, uint16_t OptNumber, const uint8_t* buf, uint16_t length) {
	if (*pOptionsListBegin == NULL) //List empty? create new first element
	{
		*pOptionsListBegin = (CoAP_option_t*) CoAP.api.malloc(sizeof(CoAP_option_t) + length);
		if (*pOptionsListBegin == NULL)
			return COAP_ERR_OUT_OF_MEMORY; //could not alloc enough mem

		(*pOptionsListBegin)->next = NULL;

		//fill data
		(*pOptionsListBegin)->Length = length;
		(*pOptionsListBegin)->Number = OptNumber;
		(*pOptionsListBegin)->Value = ((uint8_t*) (*pOptionsListBegin)) + sizeof(CoAP_option_t);
		if(NULL != ((*pOptionsListBegin)->Value) && buf != NULL)
		{
		    coap_memcpy((void*) ((*pOptionsListBegin)->Value), (const void*) buf, length);
		}
	} else //append new element at end
	{
		CoAP_option_t* pOption = *pOptionsListBegin;
		while (pOption->next != NULL)
			pOption = pOption->next;

		pOption->next = (CoAP_option_t*) CoAP.api.malloc(sizeof(CoAP_option_t) + length);
		if (pOption->next == NULL)
			return COAP_ERR_OUT_OF_MEMORY; //could not alloc enough mem

		pOption = pOption->next;
		pOption->next = NULL;

		//fill data
		pOption->Length = length;
		pOption->Number = OptNumber;
		pOption->Value = ((uint8_t*) pOption) + sizeof(CoAP_option_t);
		coap_memcpy((void*) (pOption->Value), (const void*) buf, length);
	}
	return COAP_OK;
}

CoAP_option_t* _rom CoAP_FindOptionByNumber(CoAP_Message_t* msg, uint16_t number) {
	CoAP_option_t* pOpt;
	for (pOpt = msg->pOptionsList; pOpt != NULL; pOpt = pOpt->next) {
		if (pOpt->Number == number) {
			return pOpt;
		}
	}
	return NULL;
}

CoAP_Result_t _rom CoAP_AddOption(CoAP_Message_t* pMsg, uint16_t OptNumber, uint8_t* buf, uint16_t length) {
	return CoAP_AppendOptionToList(&pMsg->pOptionsList, OptNumber, buf, length);
}

CoAP_Result_t _rom CoAP_AppendUintOptionToList(CoAP_option_t** pOptionsListBegin, uint16_t OptNumber, uint32_t val) {
	uint8_t wBuf[4];

	if(val==0)
	{
		return CoAP_AppendOptionToList(pOptionsListBegin, OptNumber ,wBuf, 0);
	}

	if(val <= 0xff)
	{
		wBuf[0]=(uint8_t)val;
		return CoAP_AppendOptionToList(pOptionsListBegin, OptNumber ,wBuf, 1);
	}

	if(val <= 0xffff)
	{
		wBuf[0]=(uint8_t)(val>>8u);
		wBuf[1]=(uint8_t)(val & 0xffu);
		return CoAP_AppendOptionToList(pOptionsListBegin, OptNumber ,wBuf, 2);
	}

	if(val <= 0xffffff)
	{
		wBuf[0]=(uint8_t)(val>>16u);
		wBuf[1]=(uint8_t)((val>>8u) & 0xffu);
		wBuf[2]=(uint8_t)(val & 0xffu);
		return CoAP_AppendOptionToList(pOptionsListBegin, OptNumber ,wBuf, 3);
	}

	wBuf[0]=(uint8_t)(val>>24u);
	wBuf[1]=(uint8_t)((val>>16u) & 0xffu);
	wBuf[2]=(uint8_t)((val>>8u) & 0xffu);
	wBuf[3]=(uint8_t)(val & 0xffu);
	return CoAP_AppendOptionToList(pOptionsListBegin, OptNumber ,wBuf, 4);
}

CoAP_Result_t _rom CoAP_GetUintFromOption(const CoAP_option_t* pOption, uint32_t* value) {
	if( value == NULL || pOption == NULL )
		return COAP_ERR_ARGUMENT;

	if( pOption->Length == 0 )
	{
		*value = 0;
	}
	else if( pOption->Length == 1 )
	{
		*value = pOption->Value[0];
	}
	else if( pOption->Length == 2 )
	{
		*value = pOption->Value[1];
		*value |= (pOption->Value[0] << 8u);
	}
	else if( pOption->Length == 3 )
	{
		*value = pOption->Value[2];
		*value |= (pOption->Value[1] << 8u);
		*value |= (pOption->Value[0] << 16u);
	}
	else if( pOption->Length == 4 )
	{
		*value = pOption->Value[3];
		*value |= (pOption->Value[2] << 8u);
		*value |= (pOption->Value[1] << 16u);
		*value |= (pOption->Value[0] << 24u);
	}
	else
	{
		return COAP_ERR_ARGUMENT;
	}

	return COAP_OK;
}

// this function adds a new option to linked list of options starting at pOptionsListBegin
// on demand the list gets reordered so that it's sorted ascending by option number (CoAP requirement)
// copies given buffer to option local buffer
CoAP_Result_t _rom CoAP_AppendOptionToList(CoAP_option_t** pOptionsListBegin, uint16_t OptNumber, const uint8_t* buf, uint16_t length) {
	if (*pOptionsListBegin == NULL) //List empty? create 1st option in list
	{
		return append_OptionToListEnd(pOptionsListBegin, OptNumber, buf, length);
	} else //try to insert the new element/option
	{
		CoAP_option_t* pOption = *pOptionsListBegin;
		while (pOption != NULL && pOption->Number <= OptNumber)
			pOption = pOption->next;

		//check reason of while end:

		//Case 1: pOption == NULL -> new number bigger than any present options numbers -> place it at end of list
		if (pOption == NULL) {
			return append_OptionToListEnd(pOptionsListBegin, OptNumber, buf, length);
		}

		//Case 2: new option has smallest number and is therefore the new start of list
		else if (pOption == *pOptionsListBegin) {
			*pOptionsListBegin = (CoAP_option_t*) CoAP.api.malloc(sizeof(CoAP_option_t) + length);
			if (*pOptionsListBegin == NULL)
				return COAP_ERR_OUT_OF_MEMORY; //could not alloc enough mem

			(*pOptionsListBegin)->next = pOption; //move former list start to 2nd pos

			//fill data
			(*pOptionsListBegin)->Length = length;
			(*pOptionsListBegin)->Number = OptNumber;
			(*pOptionsListBegin)->Value = ((uint8_t*) (*pOptionsListBegin)) + sizeof(CoAP_option_t);
			coap_memcpy((void*) ((*pOptionsListBegin)->Value), (const void*) buf, length);
		}

		//Case 3:
		//new option has to be placed anywhere in the list middle
		//pOption points to a option which Number is > than the OptNumber to be inserted
		//coap wants option numbers to be ascending - so insert the new option between pOption and its predecessor
		//The predecessor has to be found first...
		else {
			CoAP_option_t* pPrev_pOption = *pOptionsListBegin;
			while (pPrev_pOption->next != pOption)
				pPrev_pOption = pPrev_pOption->next; //search predecessor of pOption

			CoAP_option_t* newOption = (CoAP_option_t*) CoAP.api.malloc(sizeof(CoAP_option_t) + length);
			if (newOption == NULL)
				return COAP_ERR_OUT_OF_MEMORY; //could not alloc enough mem

			pPrev_pOption->next = newOption; //insert new option after predecessor
			newOption->next = pOption;

			//fill data
			newOption->Length = length;
			newOption->Number = OptNumber;
			newOption->Value = ((uint8_t*) newOption) + sizeof(CoAP_option_t);
			coap_memcpy((void*) (newOption->Value), (const void*) buf, length);
		}
	}
	return COAP_OK;
}

CoAP_Result_t _rom CoAP_CopyOptionToList(CoAP_option_t** pOptionsListBegin, CoAP_option_t* OptToCopy) {
	return CoAP_AppendOptionToList(pOptionsListBegin, OptToCopy->Number, OptToCopy->Value, OptToCopy->Length);
}

CoAP_Result_t _rom CoAP_FreeOptionList(CoAP_option_t** pOptionsListBegin) {
	if (*pOptionsListBegin == NULL)
		return COAP_OK; //any list to delete?

	CoAP_option_t* pOption1;

	pOption1 = (*pOptionsListBegin)->next; //1st element after start
	while (pOption1 != NULL) {
		//this unlinks the 2nd element by seting 1st->next to 3rd element
		(*pOptionsListBegin)->next = (*pOptionsListBegin)->next->next;
		CoAP.api.free((void*) pOption1); //free "old" 1st unlinked element
		pOption1 = (*pOptionsListBegin)->next; // (new) 1st element after start
	}

	CoAP.api.free((void*) (*pOptionsListBegin));
	*pOptionsListBegin = NULL;

	return COAP_OK;
}

/**
 * Critical Option
 *   An option that would need to be understood by the endpoint
 *   ultimately receiving the message in order to properly process the
 *   message (Section 5.4.1).  Note that the implementation of critical
 *   options is, as the name "Option" implies, generally optional:
 *   unsupported critical options lead to an error response or summary
 *   rejection of the message.
 */
uint16_t _rom CoAP_CheckForUnknownCriticalOption(CoAP_option_t* pOptionsListBegin) {
	//uses:
	//#define KNOWN_OPTIONS_COUNT (X)
	//extern uint16_t KNOWN_OPTIONS[KNOWN_OPTIONS_COUNT];
	if (pOptionsListBegin == NULL)
		return 0; //no options, nothing can be unknown

	CoAP_option_t* pOption = pOptionsListBegin;
	bool optKnown;
	do {
		optKnown = false;
		uint32_t j;
		for (j = 0; j < KNOWN_OPTIONS_COUNT; j++) {
			if (pOption->Number == KNOWN_OPTIONS[j]) {
				optKnown = true;
				continue;
			}
		}

		if (!optKnown) //check if critical
		{
			if ((pOption->Number) & OPT_FLAG_CRITICAL) {
				return pOption->Number;
			}
		}

		if (pOption->next == NULL)
			return 0;
		pOption = pOption->next;
	} while (1);

	return 0;
}

static void _rom printOptionString(const CoAP_option_t *op) {
	for (int i = 0; i < op->Length; i++) {
		char c = op->Value[i];
		if (c < 0x20 || c > 0x7e) {
			// replace non printable bytes in log by '.'
			c = '.';
		}
		LOG_DEBUG("%c", c);
	}
}
static void _rom printOptionHex(const CoAP_option_t *op) {
        LOG_DEBUG_ARRAY("Hex: ", op->Value, op->Length);
}
uint16_t CoAP_DecodeSzx(uint8_t szx) {
	if (szx > 0b110u) {
		return 0;
	}
	return 0b1u << (szx + 4u);
}
uint8_t CoAP_EncodeSzx(uint16_t blocksize) {
	switch (blocksize) {
		case 1024:
			return 0b110;
		case 512:
			return 0b101;
		case 256:
			return 0b100;
		case 128:
			return 0b011;
		case 64:
			return 0b010;
		case 32:
			return 0b001;
		default:
			return 0b000;
	}
}

#if defined(CONFIG_LOBARO_COAP_DEBUG_LEVEL) && (CONFIG_LOBARO_COAP_DEBUG_LEVEL > 1)
static const char *contentFormatMime(uint16_t c) {
	switch (c) {
		case 0:
			return "text/plain";
		case 40:
			return "application/link-format";
		case 41:
			return "application/xml";
		case 42:
			return "application/octet-stream";
		case 47:
			return "application/exi";
		case 50:
			return "application/json";
		case 60:
			// see rfc8075
			return "application/cbor";
		default:
			// default - used for unknown types, see RFC 2046 4.5.1
			return "application/octet-stream";
	}
}
#endif

static void _rom printOption(const CoAP_option_t *op) {
	if (op==NULL) {
		return;
	}
	uint32_t v, num;
	bool m;
	uint8_t szx;
	INFO("<%02u=", op->Number);

	switch (op->Number) {
		case OPT_NUM_URI_HOST:  // 03
			INFO("URI_HOST:");
			printOptionString(op);
			break;
		case OPT_NUM_ETAG:  // 04
			INFO("ETAG:");
			printOptionHex(op);
			break;
		case OPT_NUM_URI_PATH:  // 11
			INFO("URI_PATH:");
			printOptionString(op);
			break;
		case OPT_NUM_CONTENT_FORMAT:  //12
			CoAP_GetUintFromOption(op, &v);
			INFO("CONTENT_FORMAT:%"PRIu32"=%s", v, contentFormatMime(v));
			break;
		case OPT_NUM_URI_QUERY:  // 15
			INFO("URI_QUERY:");
			printOptionString(op);
			break;
		case OPT_NUM_BLOCK1:  // 27
		case OPT_NUM_BLOCK2:  // 23
			CoAP_GetUintFromOption(op, &v);
			CoAP_UnpackBlockParameter(v, &num, &m, &szx);
			INFO("BLOCK%d:%"PRIu32"""/%u/0b%u%u%u=%u",
				op->Number == OPT_NUM_BLOCK1 ? 1 : 2, num, m,
				(szx&0b100u)>2u, (szx&0b10u)>1u, (szx&0b1u), CoAP_DecodeSzx(szx));
			break;
		case OPT_NUM_SIZE1:  // 60
		case OPT_NUM_SIZE2:  // 28
			CoAP_GetUintFromOption(op, &v);
			INFO("SIZE%d:%"PRIu32, op->Number == OPT_NUM_SIZE1 ? 1 : 2, v);
			break;
		default:
			INFO("?");
			break;
	}
	INFO("> [");
	LOG_DEBUG_ARRAY("Hex: ", op->Value, op->Length);
	INFO("]");
}

void _rom CoAP_printOptionsList(CoAP_option_t* pOptListBegin) {
	uint16_t n = 0;
	while (pOptListBegin != NULL) {
		n++;
		INFO("-Option #%02u (%02d,l=%02u) -> ", n, pOptListBegin->Number, pOptListBegin->Length);
		printOption(pOptListBegin);
/*		int j;
		for (j = 0; j < pOptListBegin->Length; j++) {
			printOption(pOptListBegin);
			if (pOptListBegin->Value[j]) {
				INFO(" %c[", pOptListBegin->Value[j]);
				INFO("%02x]", pOptListBegin->Value[j]);
			} else {
				INFO("  [0x00]");
			}
		}*/
		INFO("\n\r");
		pOptListBegin = pOptListBegin->next;
	}
}

bool _rom CoAP_OptionsAreEqual(CoAP_option_t* OptA, CoAP_option_t* OptB) {
	if (OptA == NULL && OptB == NULL)
		return true;
	if (OptA == NULL || OptB == NULL)
		return false;

	//check case 4 => both != NULL:
	if (OptA->Length != OptB->Length)
		return false;
	if (OptA->Number != OptB->Number)
		return false;
	int i;
	for (i = 0; i < OptA->Length; i++) {
		if (OptA->Value[i] != OptB->Value[i])
			return false;
	}
	return true;
}

uint32_t _rom CoAP_PackBlockParameter(uint32_t num, bool m, uint8_t szx) {
	uint32_t v = num << 4u;  // v is combined: NUM[28],M[1],SZX[3]
	if (m) {
		v |= 0b1000u;
	}
	v |= szx;
	return v;
}

void CoAP_UnpackBlockParameter(uint32_t v, uint32_t *num, bool *m, uint8_t *szx) {
	*num = v >> 4u;
	*m = ((v & 0b1000u) == 0b1000u);
	*szx = v & 0b111u;
}
