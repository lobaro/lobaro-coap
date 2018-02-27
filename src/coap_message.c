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
#include <stdbool.h>
#include "coap.h"
#include "liblobaro_coap.h"

void _rom CoAP_InitToEmptyResetMsg(CoAP_Message_t* msg) {
	msg->Type = RST;
	msg->Code = EMPTY;
	msg->PayloadLength = 0;
	msg->PayloadBufSize = 0;
	msg->MessageID = 0;
	msg->pOptionsList = NULL;
	msg->Payload = NULL;
	CoAP_Token_t tok = {.Token= {0, 0, 0, 0, 0, 0, 0, 0}, .Length = 0};
	msg->Token = tok;
	msg->Timestamp = 0;
}

bool CoAP_TokenEqual(CoAP_Token_t a, CoAP_Token_t b) {
	if (a.Length != b.Length) {
		return false;
	}
	for (int i = 0; i < a.Length; i++) {
		if (a.Token[i] != b.Token[i]) {
			return false;
		}
	}
	return true;
}

void _rom CoAP_FreeMsgPayload(CoAP_Message_t* pMsg) {
	if (pMsg->Payload == NULL) {
		return;
	}

	CoAP.api.free((void*) pMsg->Payload);
	pMsg->Payload = NULL;
	pMsg->PayloadBufSize = 0;
}

bool _rom CoAP_MsgIsRequest(CoAP_Message_t* pMsg) {
	if (pMsg->Code != EMPTY && pMsg->Code <= REQ_LAST) {
		return true;
	}
	return false;
}

bool _rom CoAP_MsgIsResponse(CoAP_Message_t* pMsg) {
	if (pMsg->Code != EMPTY && pMsg->Code >= RESP_FIRST_2_00) {
		return true;
	}
	return false;
}

bool _rom CoAP_MsgIsOlderThan(CoAP_Message_t* pMsg, uint32_t timespan) {
	if (timeAfter(CoAP.api.rtc1HzCnt(), pMsg->Timestamp + timespan)) {
		return true;
	} else {
		return false;
	}
}

CoAP_Result_t _rom CoAP_FreeMessage(CoAP_Message_t* pMsg) {
	if (pMsg == NULL) {
		return COAP_OK; //nothing to free
	}

	if (pMsg->Type == CON) {
		INFO("- Message memory freed! (CON, MID: %d):\r\n", pMsg->MessageID);
	} else if ((pMsg)->Type == NON) {
		INFO("- Message memory freed! (NON, MID: %d):\r\n", pMsg->MessageID);
	} else if ((pMsg)->Type == ACK) {
		INFO("- Message memory freed! (ACK, MID: %d):\r\n", pMsg->MessageID);
	} else if ((pMsg)->Type == RST) {
		INFO("- Message memory freed! (RST, MID: %d):\r\n", pMsg->MessageID);
	}

	CoAP_FreeOptionList(pMsg->pOptionsList);
	pMsg->pOptionsList = NULL;
	CoAP_FreeMsgPayload(pMsg);

	//finally delete msg body
	CoAP.api.free(pMsg);
	return COAP_OK;
}

static CoAP_MessageType_t _rom CoAP_getRespMsgType(CoAP_Message_t* ReqMsg) { //todo inline it
	if (ReqMsg->Type == CON)
		return ACK; //for piggybacked responses
	else
		return NON;
}

static uint16_t _rom CoAP_getRespMsgID(CoAP_Message_t* ReqMsg) {
	if (ReqMsg->Type == CON)
		return ReqMsg->MessageID; //for piggybacked responses
	else
		return CoAP_GetNextMid();
}

CoAP_Message_t* _rom CoAP_CreateResponseMsg(CoAP_Message_t* reqMsg, CoAP_MessageCode_t code) {
	return CoAP_CreateMessage(CoAP_getRespMsgType(reqMsg), code, CoAP_getRespMsgID(reqMsg), reqMsg->Token);
}

CoAP_Message_t* _rom CoAP_CreateMessage(CoAP_MessageType_t type, CoAP_MessageCode_t code, uint16_t messageID, CoAP_Token_t token) {
	CoAP_Message_t* pMsg = (CoAP_Message_t*) CoAP.api.malloc(sizeof(CoAP_Message_t)); //malloc space
	if (pMsg == NULL) {
		return NULL;
	}
	memset(pMsg, 0, sizeof(CoAP_Message_t));

	CoAP_InitToEmptyResetMsg(pMsg); //init

	pMsg->Type = type;
	pMsg->Code = code;
	pMsg->MessageID = messageID;
	pMsg->Token = token;
	pMsg->Timestamp = 0;

	pMsg->PayloadLength = 0;
	pMsg->PayloadBufSize = 0;
	pMsg->Payload = NULL;

	return pMsg;
}

CoAP_Result_t _rom CoAP_ParseMessageFromDatagram(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t** rxedMsg) {
	//we use local mem and copy afterwards because we dont know yet the size of payload buffer
	//but want to allocate one block for complete final "rxedMsg" memory without realloc the buf size later.
	static CoAP_Message_t msg;

	uint8_t tokenLength = 0;

	*rxedMsg = NULL;

	if (srcArrLength < 4) {
		return COAP_PARSE_DATAGRAM_TOO_SHORT; // Minimum Size of CoAP Message = 4 Bytes
	}

	CoAP_InitToEmptyResetMsg(&msg);

//1st Header Byte
	uint8_t Version = srcArr[0] >> 6;
	if (Version != COAP_VERSION) {
		return COAP_PARSE_UNKOWN_COAP_VERSION;
	}

	msg.Type = (srcArr[0] & 0b110000) >> 4;
	tokenLength = srcArr[0] & 0b1111;
	if (tokenLength > 8) {
		INFO("CoAP-Parse Byte1 Error\r\n");
		return COAP_PARSE_MESSAGE_FORMAT_ERROR;
	} // return COAP_PARSE_MESSAGE_FORMAT_ERROR;

//2nd & 3rd Header Byte
	msg.Code = srcArr[1];

	//"Hack" to support early version of "myCoAP" iOS app which sends malformed "CoAP-pings" containing a token...
	//if(Msg.Code == EMPTY && (TokenLength != 0 || srcArrLength != 4))	{INFO("err2\r\n");return COAP_PARSE_MESSAGE_FORMAT_ERROR;}// return COAP_PARSE_MESSAGE_FORMAT_ERROR;

	uint8_t codeClass = ((uint8_t) msg.Code) >> 5;
	if (codeClass == 1 || codeClass == 6 || codeClass == 7) {
		INFO("CoAP-Parse Byte2/3 Error\r\n");
		return COAP_PARSE_MESSAGE_FORMAT_ERROR;
	}    //  return COAP_PARSE_MESSAGE_FORMAT_ERROR; //reserved classes

//4th Header Byte
	msg.MessageID = (uint16_t) srcArr[2] << 8 | srcArr[3];

//further parsing locations depend on parsed 4Byte CoAP Header -> use of offset addressing
	uint16_t offset = 4;
	if (srcArrLength == offset) //no more data -> maybe a CoAP Ping
	{
		goto START_MSG_COPY_LABEL;
		//quick end of parsing...
	}

//Token (if any)
	CoAP_Token_t tok = {.Token = {0, 0, 0, 0, 0, 0, 0, 0}, .Length = tokenLength};
	msg.Token = tok;
	int i;
	for (i = 0; i < tokenLength; i++) {
		msg.Token.Token[i] = srcArr[offset + i];
	}

	offset += tokenLength;
	if (srcArrLength == offset)
		goto START_MSG_COPY_LABEL;

//Options (if any)
	uint8_t* pPayloadBegin = NULL;

	//this allocates memory for every option and puts it in die pOptionsList linked list
	//start address of payload also given back
	CoAP_Result_t ParseOptionsResult = parse_OptionsFromRaw(&(srcArr[offset]), srcArrLength - offset, &pPayloadBegin, &(msg.pOptionsList));

	if (ParseOptionsResult != COAP_OK) {
		CoAP_FreeOptionList(msg.pOptionsList);
		msg.pOptionsList = NULL;
		INFO("CoAP-Parse Options Error\r\n");
		return ParseOptionsResult;
	}

//Payload (if any)
	if (pPayloadBegin != NULL) {
		msg.PayloadLength = srcArrLength - (pPayloadBegin - srcArr);
		if (msg.PayloadLength > MAX_PAYLOAD_SIZE) {
			CoAP_FreeOptionList(msg.pOptionsList);
			msg.pOptionsList = NULL;
			return COAP_PARSE_TOO_MUCH_PAYLOAD;
		}
	} else
		msg.PayloadLength = 0;

	msg.PayloadBufSize = msg.PayloadLength;

//Get memory for total message data and copy parsed data
//Payload Buffers MUST located at end of CoAP_Message_t to let this work!
	START_MSG_COPY_LABEL:
	*rxedMsg = (CoAP_Message_t*) CoAP.api.malloc(sizeof(CoAP_Message_t) + msg.PayloadLength);
	// TODO: We MUST malloc the payload separately!

	if (*rxedMsg == NULL)    //out of memory
	{
		CoAP_FreeOptionList(msg.pOptionsList);
		msg.pOptionsList = NULL;
		return COAP_ERR_OUT_OF_MEMORY;
	}

	coap_memcpy(*rxedMsg, &msg, sizeof(CoAP_Message_t));

	if (msg.PayloadLength) {
		(*rxedMsg)->Payload = ((uint8_t*) (*rxedMsg)) + sizeof(CoAP_Message_t);
		coap_memcpy((*rxedMsg)->Payload, pPayloadBegin, msg.PayloadLength);
	}

	(*rxedMsg)->Timestamp = CoAP.api.rtc1HzCnt();

	return COAP_OK;
}

int CoAP_GetRawSizeOfMessage(CoAP_Message_t* pMsg) {
	int totalMsgBytes = 0;

	totalMsgBytes += 4; //Header

	totalMsgBytes += CoAP_NeededMem4PackOptions(pMsg->pOptionsList);

	if (pMsg->Code != EMPTY) {

		if (pMsg->PayloadLength) {
			totalMsgBytes += pMsg->PayloadLength + 1; //+1 = PayloadMarker
		}

		totalMsgBytes += pMsg->Token.Length;
	}

	return totalMsgBytes;
}

CoAP_Result_t _rom CoAP_BuildDatagram(uint8_t* destArr, uint16_t* pDestArrSize, CoAP_Message_t* pMsg) {
	uint16_t offset = 0;
	uint8_t tokenLength;

	if (pMsg->Code == EMPTY) { //must send only 4 byte header overwrite upper layer in any case!
		pMsg->PayloadLength = 0;
		tokenLength = 0;
	} else {
		tokenLength = pMsg->Token.Length;
	}

// 4Byte Header (see p.16 RFC7252)
	destArr[0] = 0;
	destArr[0] |= (COAP_VERSION & 3) << 6;
	destArr[0] |= (pMsg->Type & 3) << 4;
	destArr[0] |= (tokenLength & 15);
	destArr[1] = (uint8_t) pMsg->Code;
	destArr[2] = (uint8_t) (pMsg->MessageID >> 8);
	destArr[3] = (uint8_t) (pMsg->MessageID & 0xff);

	offset += 4;

// Token (0 to 8 Bytes)
	int i;
	for (i = 0; i < tokenLength; i++) {
		destArr[offset + i] = pMsg->Token.Token[i];
	}
	offset += tokenLength;

// Options
	if (pMsg->pOptionsList != NULL) {
		uint16_t OptionsRawByteCount = 0;
		//iterates through (ascending sorted!) list of options and encodes them in CoAPs compact binary representation
		pack_OptionsFromList(&(destArr[offset]), &OptionsRawByteCount, pMsg->pOptionsList);

		offset += OptionsRawByteCount;
	}

//Payload
	if (pMsg->PayloadLength != 0) {
		destArr[offset] = 0xff; //Payload Marker
		offset++;

		coap_memcpy((void*) &(destArr[offset]), (void*) (pMsg->Payload), pMsg->PayloadLength);

		offset += pMsg->PayloadLength;
	}

	*pDestArrSize = offset; // => Size of Datagram array
	return COAP_OK;
}


//todo use random and hash
uint16_t _rom CoAP_GetNextMid() {
	static uint16_t MId = 0;
	MId++;
	return MId;
}

// TODO: Improove generated tokens
CoAP_Token_t _rom CoAP_GenerateToken() {
	static uint8_t currToken = 0x44;
	currToken++;
	CoAP_Token_t tok = {.Token = {currToken, 0, 0, 0, 0, 0, 0, 0}, .Length = 1};
	return tok;
}

CoAP_Result_t _rom CoAP_addNewPayloadToMessage(CoAP_Message_t* Msg, uint8_t* pData, uint16_t size) {
	if (size > MAX_PAYLOAD_SIZE) {
		ERROR("payload > MAX_PAYLOAD_SIZE");
		return COAP_ERR_OUT_OF_MEMORY;
	}

	if (size > 0) {
		if (Msg->PayloadBufSize >= size) {
			coap_memcpy(Msg->Payload, pData, size); //use existing buffer
		} else {
			CoAP_FreeMsgPayload(Msg); //free old buffer
			Msg->Payload = (uint8_t*) CoAP.api.malloc(size); //alloc a different new buffer
			Msg->PayloadBufSize = size;

			coap_memcpy(Msg->Payload, pData, size);
		}
	} else {
		CoAP_FreeMsgPayload(Msg);
	}

	Msg->PayloadLength = size;

	return COAP_OK;
}

CoAP_Result_t _rom CoAP_addTextPayload(CoAP_Message_t* Msg, char* PayloadStr) {
	return CoAP_addNewPayloadToMessage(Msg, (uint8_t*) PayloadStr, (uint16_t) (strlen(PayloadStr)));
}

void _rom CoAP_PrintMsg(CoAP_Message_t* msg) {
	INFO("---------CoAP msg--------\r\n");

	if (msg->Type == CON) {
		LOG_DEBUG("*Type: CON (0x%02x)\r\n", msg->Type);
	} else if (msg->Type == NON) {
		LOG_DEBUG("*Type: NON (0x%02x)\r\n", msg->Type);
	} else if (msg->Type == ACK) {
		LOG_DEBUG("*Type: ACK (0x%02x)\r\n", msg->Type);
	} else if (msg->Type == RST) {
		LOG_DEBUG("*Type: RST (0x%02x)\r\n", msg->Type);
	} else {
		LOG_DEBUG("*Type: UNKNOWN! (0x%02x)\r\n", msg->Type);
	}

	uint8_t tokenBytes = msg->Token.Length;
	if (tokenBytes > 0) {
		LOG_DEBUG("*Token: %u Byte -> 0x", tokenBytes);
		int i;
		for (i = 0; i < tokenBytes; i++) {
			LOG_DEBUG("%02x", msg->Token.Token[i]);
		}
	} else {
		LOG_DEBUG("*Token: %u Byte -> 0", tokenBytes);
	}

	uint8_t code = msg->Code;
	LOG_DEBUG("\r\n*Code: %d.%02d (0x%02x) ", code >> 5, code & 31, code);

	if (msg->Code == EMPTY) {
		LOG_DEBUG("[EMPTY]\r\n");
	} else if (msg->Code == REQ_GET) {
		LOG_DEBUG("[REQ_GET]\r\n");
	} else if (msg->Code == REQ_POST) {
		LOG_DEBUG("[REQ_POST]\r\n");
	} else if (msg->Code == REQ_PUT) {
		LOG_DEBUG("[REQ_PUT]\r\n");
	} else if (msg->Code == REQ_DELETE) {
		LOG_DEBUG("[REQ_DELETE]\r\n");
	} else
		LOG_DEBUG("\r\n");

	LOG_DEBUG("*MessageId: %u\r\n", msg->MessageID);

	CoAP_option_t* pOption = NULL;
	if (msg->pOptionsList != NULL) {
		pOption = msg->pOptionsList;
	}
	while (pOption != NULL) {
		INFO("*Option #%u (Length=%u) ->", pOption->Number, pOption->Length);
		int j;
		for (j = 0; j < pOption->Length; j++) {
			if (pOption->Value[j]) {
				INFO(" %c[", pOption->Value[j]);
				INFO("%02x]", pOption->Value[j]);
			} else {
				INFO("  [00]", pOption->Value[j]);
			}
		}
		INFO("\r\n");
		pOption = pOption->next;
	}
	if (msg->PayloadLength) {
		LOG_DEBUG("*Payload (%u Byte): \"", msg->PayloadLength);
		if (msg->PayloadLength > MAX_PAYLOAD_SIZE) {
			LOG_DEBUG("too much payload!");
		} else {
			int i;
			for (i = 0; i < msg->PayloadLength && i < MAX_PAYLOAD_SIZE; i++) {
				LOG_DEBUG("%c", msg->Payload[i]);
			}
		}
		LOG_DEBUG("\"\r\n");
	}

	INFO("*Timestamp: %d\r\n", msg->Timestamp);
	INFO("----------------------------\r\n");
}

void _rom CoAP_PrintResultValue(CoAP_Result_t res) {
	if (res == COAP_OK) {
		INFO("COAP_OK\r\n");
	} else if (res == COAP_PARSE_DATAGRAM_TOO_SHORT) {
		INFO("COAP_PARSE_DATAGRAM_TOO_SHORT\r\n");
	} else if (res == COAP_PARSE_UNKOWN_COAP_VERSION) {
		INFO("COAP_PARSE_UNKOWN_COAP_VERSION\r\n");
	} else if (res == COAP_PARSE_MESSAGE_FORMAT_ERROR) {
		INFO("COAP_PARSE_MESSAGE_FORMAT_ERROR\r\n");
	} else if (res == COAP_PARSE_TOO_MANY_OPTIONS) {
		INFO("COAP_PARSE_TOO_MANY_OPTIONS\r\n");
	} else if (res == COAP_PARSE_TOO_LONG_OPTION) {
		INFO("COAP_PARSE_TOO_LONG_OPTION\r\n");
	} else if (res == COAP_PARSE_TOO_MUCH_PAYLOAD) {
		INFO("COAP_PARSE_TOO_MUCH_PAYLOAD\r\n");
	} else if (res == COAP_ERR_OUT_OF_MEMORY) {
		INFO("COAP_ERR_OUT_OF_MEMORY\r\n");
	} else {
		INFO("UNKNOWN RESULT\r\n");
	}
}
