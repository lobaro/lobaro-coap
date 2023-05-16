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
#include <inttypes.h>
#include "coap.h"
#include "liblobaro_coap.h"
#include "coap_mem.h"

static void _rom CoAP_InitToEmptyResetMsg(CoAP_Message_t* msg) {
	msg->Type = RST;
	msg->Code = MSG_EMPTY;
	msg->PayloadLength = 0;
	msg->PayloadBufSize = 0;
	msg->MessageID = 0;
	msg->pOptionsList = NULL;
	msg->Payload = NULL;
	CoAP_Token_t tok = {.Token= {0,0,0,0,0,0,0,0}, .Length = 0};
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

void _rom CoAP_free_MsgPayload(CoAP_Message_t** Msg) {
	if ((NULL == (*Msg)->Payload) || (0 == (*Msg)->PayloadBufSize))
		return;

	CoAP.api.free((void*) (*Msg)->Payload);
	(*Msg)->Payload = NULL;
	(*Msg)->PayloadBufSize = 0;
}

bool _rom CoAP_MsgIsRequest(CoAP_Message_t* pMsg) {
	if (pMsg->Code != MSG_EMPTY && pMsg->Code <= REQ_LAST)
		return true;
	return false;
}

bool _rom CoAP_MsgIsResponse(CoAP_Message_t* pMsg) {
	if (pMsg->Code != MSG_EMPTY && pMsg->Code >= RESP_FIRST_2_00)
		return true;
	return false;
}

bool _rom CoAP_MsgIsOlderThan(CoAP_Message_t* pMsg, uint32_t timespan) {
	if (timeAfter(CoAP.api.rtc1HzCnt(), pMsg->Timestamp + timespan)) {
		return true;
	}
	else {
		return false;
	}
}

CoAP_Result_t _rom CoAP_free_Message(CoAP_Message_t** Msg) {
	DEBUG("Free message %p\n\r", *Msg);
	if (*Msg == NULL) {
		return COAP_OK; //nothing to free
	}

	if ((*Msg)->Type == CON) {
		DEBUG("- Message memory freed! (CON, MID: %d):\r\n", (*Msg)->MessageID);
	}
	else if ((*Msg)->Type == NON) {
		DEBUG("- Message memory freed! (NON, MID: %d):\r\n", (*Msg)->MessageID);
	}
	else if ((*Msg)->Type == ACK) {
		DEBUG("- Message memory freed! (ACK, MID: %d):\r\n", (*Msg)->MessageID);
	}
	else if ((*Msg)->Type == RST) {
		DEBUG("- Message memory freed! (RST, MID: %d):\r\n", (*Msg)->MessageID);
	}

	CoAP_FreeOptionList(&((*Msg)->pOptionsList));
	CoAP_free_MsgPayload(Msg);

	//finally delete msg body
	CoAP_free((void*) (*Msg));
	*Msg = NULL;

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

CoAP_Message_t* _rom CoAP_AllocRespMsg(CoAP_Message_t* ReqMsg, CoAP_MessageCode_t Code, uint16_t PayloadMaxSize) {
	return CoAP_CreateMessage(CoAP_getRespMsgType(ReqMsg), Code, CoAP_getRespMsgID(ReqMsg), NULL, 0, PayloadMaxSize, ReqMsg->Token);
}

CoAP_Message_t* _rom CoAP_CreateMessage(CoAP_MessageType_t Type,
		CoAP_MessageCode_t Code,
		uint16_t MessageID,
		const uint8_t* pPayloadInitialContent,
		uint16_t PayloadInitialContentLength,
		uint16_t PayloadMaxSize,
		CoAP_Token_t Token) {

	//safety checks
	if (PayloadInitialContentLength > PayloadMaxSize) {
		ERROR("Initial Content bigger than field size!");
		return NULL;
	}

	CoAP_Message_t* pMsg = (CoAP_Message_t*) CoAP_malloc0(sizeof(CoAP_Message_t));
	if (pMsg == NULL) {
		return NULL;
	}

	CoAP_InitToEmptyResetMsg(pMsg); //init

	if (PayloadMaxSize) {
		pMsg->Payload = CoAP_malloc0(PayloadMaxSize);
		if (NULL == pMsg->Payload) {
			return NULL;
		}
		pMsg->PayloadBufSize = PayloadMaxSize;
		pMsg->PayloadLength = PayloadInitialContentLength;
		if (pPayloadInitialContent != NULL) {
			coap_memcpy((void*) ((pMsg)->Payload), (const void*) pPayloadInitialContent, PayloadInitialContentLength);
		}
	}
	INFO("Created message %p\n\r", pMsg);

	pMsg->Type = Type;
	pMsg->Code = Code;
	pMsg->MessageID = MessageID;
	pMsg->Token = Token;
	pMsg->Timestamp = 0;

	return pMsg;
}

CoAP_Result_t _rom CoAP_ParseDatagramUpToToken(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t* Msg, uint16_t *optionsOfsset) {
	uint8_t TokenLength = 0;

	if (srcArrLength < 4)
		return COAP_PARSE_DATAGRAM_TOO_SHORT; // Minimum Size of CoAP Message = 4 Bytes

	CoAP_InitToEmptyResetMsg(Msg);

//1st Header Byte
	uint8_t Version = srcArr[0] >> 6u;
	if (Version != COAP_VERSION)
		return COAP_PARSE_UNKOWN_COAP_VERSION;

	Msg->Type = (srcArr[0] & 0x30u) >> 4u;
	TokenLength = srcArr[0] & 0xFu;
	if (TokenLength > 8) {
		INFO("CoAP-Parse Byte1 Error\r\n");
		return COAP_PARSE_MESSAGE_FORMAT_ERROR;
	} 

//2nd & 3rd Header Byte
	Msg->Code = srcArr[1];

	//"Hack" to support early version of "myCoAP" iOS app which sends malformed "CoAP-pings" containing a token...
	//if(Msg->Code == MSG_EMPTY && (TokenLength != 0 || srcArrLength != 4))	{INFO("err2\r\n");return COAP_PARSE_MESSAGE_FORMAT_ERROR;}// return COAP_PARSE_MESSAGE_FORMAT_ERROR;

	uint8_t codeClass = ((uint8_t) Msg->Code) >> 5u;
	if (codeClass == 1 || codeClass == 6 || codeClass == 7) {
		INFO("CoAP-Parse Byte2/3 Error\r\n");
		return COAP_PARSE_MESSAGE_FORMAT_ERROR;
	}

//4th Header Byte
	Msg->MessageID = (uint16_t) srcArr[2] << 8u | srcArr[3];

//further parsing locations depend on parsed 4Byte CoAP Header -> use of offset addressing
	uint16_t offset = 4;
	if (srcArrLength == offset) //no more data -> maybe a CoAP Ping
	{
		//quick end of parsing...
		*optionsOfsset = offset;
		return COAP_OK;
	}

//Token (if any)
	CoAP_Token_t tok = {.Token = {0,0,0,0,0,0,0,0}, .Length = TokenLength};
	Msg->Token = tok;
	int i;
	for (i = 0; i < TokenLength; i++) {
		Msg->Token.Token[i] = srcArr[offset + i];
	}

	offset += TokenLength;
	*optionsOfsset = offset;
	return COAP_OK;
}

CoAP_Result_t CoAP_PrepareResponseWithEcho(CoAP_Message_t* msg, uint8_t* echoValue, size_t echoValueLength) {
	msg->Type = CoAP_getRespMsgType(msg);
	msg->Code = RESP_ERROR_UNAUTHORIZED_4_01;
	// CoAP_ParseDatagramUpToToken doesn't parse options and payload - they are initialized as 0, as required by this response
	// Message ID and Token are copied from the request, as required.

	return CoAP_AddOption(msg, OPT_NUM_ECHO, echoValue, echoValueLength);
}

CoAP_Result_t _rom CoAP_ParseMessageFromDatagram(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t** rxedMsg) {
	//we use local mem and copy afterwards because we dont know yet the size of payload buffer
	//but want to allocate one block for complete final "rxedMsg" memory without realloc the buf size later.
	static CoAP_Message_t Msg;
	uint16_t offset=0;
	CoAP_Result_t result = CoAP_ParseDatagramUpToToken(srcArr, srcArrLength, &Msg, &offset);
	if(COAP_OK != result){
		return result;
	}
//Options (if any)
	uint8_t* pPayloadBegin = NULL;

	//this allocates memory for every option and puts it in die pOptionsList linked list
	//start address of payload also given back
	CoAP_Result_t ParseOptionsResult = parse_OptionsFromRaw(&(srcArr[offset]), srcArrLength - offset, &pPayloadBegin, &(Msg.pOptionsList));

	if (ParseOptionsResult != COAP_OK) {
		CoAP_FreeOptionList(&(Msg.pOptionsList));
		INFO("CoAP-Parse Options Error\r\n");
		return ParseOptionsResult;
	}

//Payload (if any)
	if (pPayloadBegin != NULL) {
		Msg.PayloadLength = srcArrLength - (pPayloadBegin - srcArr);
		if (Msg.PayloadLength > MAX_PAYLOAD_SIZE) {
			// do not return early - this problem can be handled gracefully.
			result = COAP_PARSE_TOO_MUCH_PAYLOAD;
			Msg.Payload = NULL;
			Msg.PayloadLength = 0;
		} else {
			Msg.Payload = pPayloadBegin;
		}
	} else
		Msg.PayloadLength = 0;

	Msg.PayloadBufSize = Msg.PayloadLength;

//Get memory for total message data and copy parsed data
	*rxedMsg = CoAP_CreateMessage( Msg.Type,
									   Msg.Code,
									   Msg.MessageID,
									   Msg.Payload,
									   Msg.PayloadLength,
									   Msg.PayloadBufSize,
									   Msg.Token );

		if (*rxedMsg == NULL ) //out of memory
		{
			CoAP_FreeOptionList( &( Msg.pOptionsList ) );
			return COAP_ERR_OUT_OF_MEMORY;
		}

		(*rxedMsg )->pOptionsList = Msg.pOptionsList;
	(*rxedMsg)->Timestamp = CoAP.api.rtc1HzCnt();

	return result;
}

int CoAP_GetRawSizeOfMessage(CoAP_Message_t* Msg) {
	int TotalMsgBytes = 0;

	TotalMsgBytes += 4; //Header

	TotalMsgBytes += CoAP_NeededMem4PackOptions(Msg->pOptionsList);

	if (Msg->Code != MSG_EMPTY) {

		if (Msg->PayloadLength) {
			TotalMsgBytes += Msg->PayloadLength + 1; //+1 = PayloadMarker
		}

		TotalMsgBytes += Msg->Token.Length;
	}

	return TotalMsgBytes;
}

CoAP_Result_t _rom CoAP_BuildDatagram(uint8_t* destArr, uint16_t* pDestArrSize, CoAP_Message_t* Msg) {
	uint16_t offset = 0;
	uint8_t TokenLength;

	if (Msg->Code == MSG_EMPTY) { //must send only 4 byte header overwrite upper layer in any case!
		Msg->PayloadLength = 0;
		TokenLength = 0;
	} else {
		TokenLength = Msg->Token.Length;
	}

// 4Byte Header (see p.16 RFC7252)
	destArr[0] = 0;
	destArr[0] |= (COAP_VERSION & 3u) << 6u;
	destArr[0] |= (Msg->Type & 3u) << 4u;
	destArr[0] |= (TokenLength & 15u);
	destArr[1] = (uint8_t) Msg->Code;
	destArr[2] = (uint8_t) (Msg->MessageID >> 8u);
	destArr[3] = (uint8_t) (Msg->MessageID & 0xffu);

	offset += 4;

// Token (0 to 8 Bytes)
	int i;
	for (i = 0; i < TokenLength; i++) {
		destArr[offset + i] = Msg->Token.Token[i];
	}
	offset += TokenLength;

// Options
	if (Msg->pOptionsList != NULL) {
		uint16_t OptionsRawByteCount = 0;
		//iterates through (ascending sorted!) list of options and encodes them in CoAPs compact binary representation
		pack_OptionsFromList(&(destArr[offset]), &OptionsRawByteCount, Msg->pOptionsList);

		offset += OptionsRawByteCount;
	}

//Payload
	if (Msg->PayloadLength != 0) {
		destArr[offset] = 0xff; //Payload Marker
		offset++;

		coap_memcpy((void*) &(destArr[offset]), (void*) (Msg->Payload), Msg->PayloadLength);

		offset += Msg->PayloadLength;
	}

	*pDestArrSize = offset; // => Size of Datagram array
	return COAP_OK;
}

//send minimal 4Byte header CoAP empty ACK message
CoAP_Result_t _rom CoAP_SendEmptyAck(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)

	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = ACK;
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

CoAP_Result_t _rom CoAP_SendResponseWithoutPayload(CoAP_MessageCode_t code, CoAP_Message_t *request, SocketHandle_t socketHandle, NetEp_t receiver, CoAP_option_t *pOptionsList){
	CoAP_Message_t Msg;
	CoAP_InitToEmptyResetMsg(&Msg);
	if(request->Type == CON)
	{
		Msg.Type = ACK;
		Msg.MessageID = request->MessageID;
	} else {
		Msg.Type = NON;
		Msg.MessageID = CoAP_GetNextMid();
	}
	Msg.Code = code;
	Msg.Token = request->Token;
	Msg.pOptionsList = pOptionsList;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

//send short response
CoAP_Result_t _rom CoAP_SendShortResp(CoAP_MessageType_t Type, CoAP_MessageCode_t Code, uint16_t MessageID, CoAP_Token_t token, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = Type;
	Msg.MessageID = MessageID;
	Msg.Code = Code;
	Msg.Token = token;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

//send minimal 4Byte header CoAP empty RST message
CoAP_Result_t _rom CoAP_SendEmptyRST(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

CoAP_Result_t _rom CoAP_SendMsg(CoAP_Message_t* Msg, SocketHandle_t socketHandle, NetEp_t receiver) {
	INFO("Sending CoAP msg\r\n");
	uint16_t bytesToSend = 0;
	CoAP_Socket_t* pSocket = RetrieveSocket(socketHandle);

	if (pSocket == NULL) {
		ERROR("Socket not found! handle: %p\r\n", socketHandle);
		return COAP_NOT_FOUND;
	}

	NetTransmit_fn SendPacket = pSocket->Tx;
	uint8_t quickBuf[16]; //speed up sending of tiny messages

	if (SendPacket == NULL) {
		ERROR("SendPacket function not found! handle: %p\r\n", socketHandle);
		return COAP_NOT_FOUND;
	}

	// build generic packet
	NetPacket_t pked;
	pked.remoteEp = receiver;

	//Alloc raw memory
	pked.size = CoAP_GetRawSizeOfMessage(Msg);
	if (pked.size <= 16) { //for small messages don't take overhead of mem allocation
		pked.pData = quickBuf;
	} else {
		pked.pData = (uint8_t*) CoAP_malloc(pked.size);
		if (pked.pData == NULL)
			return COAP_ERR_OUT_OF_MEMORY;
	}

	// serialize msg
	CoAP_BuildDatagram((pked.pData), &bytesToSend, Msg);

	if (bytesToSend != pked.size) {
		INFO("(!!!) Bytes to Send = %d estimated = %d\r\n", bytesToSend, CoAP_GetRawSizeOfMessage(Msg));
	}

	INFO("\r\no>>>>>>>>>>>>>>>>>>>>>>\r\nSend Message [%d Bytes], Interface #%p\r\n", bytesToSend, socketHandle);
	INFO("Receiving Endpoint: ");
	PrintEndpoint(&(pked.remoteEp));
	INFO("\n\r");
	LOG_DEBUG_ARRAY("HEX: ", pked.pData, pked.size);
	DEBUG("\r\n");

	bool sendResult;
#if DEBUG_RANDOM_DROP_OUTGOING_PERCENTAGE > 0
	if (CoAP.api.rand() % 100 < DEBUG_RANDOM_DROP_OUTGOING_PERCENTAGE) {
		INFO("!!!FAIL!!! on purpose, dropping outgoing message (%d%% chance)\n", DEBUG_RANDOM_DROP_OUTGOING_PERCENTAGE);
		sendResult = true;  // make stack think it sent message, to simulate loss of UDP packet in network
	} else {
		sendResult = SendPacket(socketHandle, &pked);
	}
#else
	sendResult = SendPacket(socketHandle, &pked);
#endif

	if (sendResult == true) { // send COAP_OK!
		Msg->Timestamp = CoAP.api.rtc1HzCnt();
		CoAP_PrintMsg(Msg);
		INFO("o>>>>>>>>>>OK>>>>>>>>>>\r\n");
		if (pked.pData != quickBuf) {
			CoAP_free(pked.pData);
		}
		return COAP_OK;
	} else {
		CoAP_PrintMsg(Msg);
		INFO("o>>>>>>>>>>FAIL>>>>>>>>>>\r\n");
		if (pked.pData != quickBuf) {
			CoAP_free(pked.pData);
		}
		return COAP_ERR_NETWORK;
	}

}

static uint16_t MId = 0;
static uint8_t currToken = 0;

void _rom CoAP_InitIds() {
	// Initialise Message-ID and Token with random values:
	MId = CoAP.api.rand() & 0xffffu;
	currToken = CoAP.api.rand() & 0xffu;
}

uint16_t _rom CoAP_GetNextMid() {
	MId++;
	return MId;
}

// TODO: Improove generated tokens
CoAP_Token_t _rom CoAP_GenerateToken() {
	currToken++;
	CoAP_Token_t tok = {.Token = {currToken, 0,0,0,0,0,0,0}, .Length = 1};
	return tok;
}

CoAP_Result_t _rom CoAP_addNewPayloadToMessage(CoAP_Message_t* Msg, uint8_t* pData, uint16_t size) {
	if (size > MAX_PAYLOAD_SIZE) {
		ERROR("payload > MAX_PAYLOAD_SIZE");
		return COAP_ERR_OUT_OF_MEMORY;
	}

	if (size) {
		if (Msg->PayloadBufSize >= size) {
			coap_memcpy(Msg->Payload, pData, size); //use existing buffer
		} else { // will move payload buf outside of msg memory frame!
			CoAP_free_MsgPayload(&Msg); //free old buffer
			Msg->Payload = (uint8_t*) CoAP.api.malloc(size); //alloc a different new buffer
			Msg->PayloadBufSize = size;

			coap_memcpy(Msg->Payload, pData, size);
		}
	} else {
		CoAP_free_MsgPayload(&Msg);
	}

	Msg->PayloadLength = size;

	return COAP_OK;
}

CoAP_Result_t _rom CoAP_addTextPayload(CoAP_Message_t* Msg, char* PayloadStr) {
	return CoAP_addNewPayloadToMessage(Msg, (uint8_t*) PayloadStr, (uint16_t) (strlen(PayloadStr)));
}

void _rom CoAP_PrintMsg(CoAP_Message_t* msg) {

#if defined(COAP_LL_INFO) || defined(COAP_LL_DEBUG)
		// Short version
		LOG_INFO("CoAP msg: Type=");
		
		switch(msg->Type)
		{
			case CON: LOG_INFO("CON(0x%02x)", msg->Type); break;
			case NON: LOG_INFO("NON(0x%02x)", msg->Type); break;
			case ACK: LOG_INFO("ACK(0x%02x)", msg->Type); break;
			case RST: LOG_INFO("RST(0x%02x)", msg->Type); break;
			default: LOG_INFO("UNKNOWN (0x%02x)", msg->Type); break;
		}

		LOG_INFO(" Code=%s", CoAP_CodeName(msg->Code));
		LOG_INFO(" MsgId=%"PRIu16, msg->MessageID);
        LOG_INFO(" Token=0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", msg->Token.Token[0], msg->Token.Token[1], msg->Token.Token[2], msg->Token.Token[3], msg->Token.Token[4], msg->Token.Token[5], msg->Token.Token[6], msg->Token.Token[7]);
		LOG_INFO(" Timestamp=%"PRIu32, msg->Timestamp);
		LOG_INFO(" PayloadLen=%"PRIu16, msg->PayloadLength);
		LOG_INFO("\n\r");
		return;
#endif

	INFO("---------CoAP msg--------\r\n");

	if (msg->Type == CON) {
		LOG_DEBUG("*Type: CON (0x%02x)\r\n", msg->Type);
	}
	else if (msg->Type == NON) {
		LOG_DEBUG("*Type: NON (0x%02x)\r\n", msg->Type);
	}
	else if (msg->Type == ACK) {
		LOG_DEBUG("*Type: ACK (0x%02x)\r\n", msg->Type);
	}
	else if (msg->Type == RST) {
		LOG_DEBUG("*Type: RST (0x%02x)\r\n", msg->Type);
	}
	else {
		LOG_DEBUG("*Type: UNKNOWN! (0x%02x)\r\n", msg->Type);
	}

	uint8_t tokenBytes = msg->Token.Length;
	if (tokenBytes > 0) {
		LOG_DEBUG("*Token: %u Byte -> ", tokenBytes);
		LOG_DEBUG_ARRAY("", msg->Token.Token, tokenBytes);
	} else {
		LOG_DEBUG("*Token: %u Byte -> 0", tokenBytes);
	}

	LOG_DEBUG("\r\n*Code: %d.%02d (0x%02x) [%s]\r\n",
			msg->Code >> 5u, msg->Code & 31u, msg->Code, CoAP_CodeName(msg->Code));

	LOG_DEBUG("*MessageId: %u\r\n", msg->MessageID);

	CoAP_printOptionsList(msg->pOptionsList);
	if (msg->PayloadLength) {
		LOG_DEBUG("*Payload (%u Byte): \r\n", msg->PayloadLength);
		if (msg->PayloadLength > MAX_PAYLOAD_SIZE) {
			LOG_DEBUG(" too much payload!\r\n");
		}
		else {

			LOG_DEBUG_ARRAY("Hex: ", msg->Payload, msg->PayloadLength);
			LOG_DEBUG("\"\r\n");
		}
	}

	INFO("*Timestamp: %"PRIu32"\r\n", msg->Timestamp);
	INFO("----------------------------\r\n");
}

const char _rom *CoAP_CodeName(CoAP_MessageCode_t code) {
	switch (code) {
		case MSG_EMPTY:
			return "EMPTY";
		case REQ_GET:
			return "REQ_GET";
		case REQ_POST:
			return "REQ_POST";
		case REQ_PUT:
			return "REQ_PUT";
		case REQ_DELETE:
			return "REQ_DELETE";
		case REQ_FETCH:
			return "REQ_FETCH";
		case REQ_PATCH:
			return "REQ_PATCH";
		case REQ_IPATCH:
			// iPATCH and LAST both 0.07
			return "REQ_IPATCH/REQ_LAST";
		case RESP_FIRST_2_00:
			return "RESP_FIRST_2_00";
		case RESP_SUCCESS_CREATED_2_01:
			return "RESP_SUCCESS_CREATED_2_01";
		case RESP_SUCCESS_DELETED_2_02:
			return "RESP_SUCCESS_DELETED_2_02";
		case RESP_SUCCESS_VALID_2_03:
			return "RESP_SUCCESS_VALID_2_03";
		case RESP_SUCCESS_CHANGED_2_04:
			return "RESP_SUCCESS_CHANGED_2_04";
		case RESP_SUCCESS_CONTENT_2_05:
			return "RESP_SUCCESS_CONTENT_2_05";
		case RESP_SUCCESS_CONTINUE_2_31:
			return "RESP_SUCCESS_CONTINUE_2_31";
		case RESP_ERROR_BAD_REQUEST_4_00:
			return "RESP_ERROR_BAD_REQUEST_4_00";
		case RESP_ERROR_UNAUTHORIZED_4_01:
			return "RESP_ERROR_UNAUTHORIZED_4_01";
		case RESP_BAD_OPTION_4_02:
			return "RESP_BAD_OPTION_4_02";
		case RESP_FORBIDDEN_4_03:
			return "RESP_FORBIDDEN_4_03";
		case RESP_NOT_FOUND_4_04:
			return "RESP_NOT_FOUND_4_04";
		case RESP_METHOD_NOT_ALLOWED_4_05:
			return "RESP_METHOD_NOT_ALLOWED_4_05";
		case RESP_METHOD_NOT_ACCEPTABLE_4_06:
			return "RESP_METHOD_NOT_ACCEPTABLE_4_06";
		case RESP_REQUEST_ENTITY_INCOMPLETE_4_08:
			return "RESP_REQUEST_ENTITY_INCOMPLETE_4_08";
		case RESP_PRECONDITION_FAILED_4_12:
			return "RESP_PRECONDITION_FAILED_4_12";
		case RESP_REQUEST_ENTITY_TOO_LARGE_4_13:
			return "RESP_REQUEST_ENTITY_TOO_LARGE_4_13";
		case RESP_UNSUPPORTED_CONTENT_FORMAT_4_15:
			return "RESP_UNSUPPORTED_CONTENT_FORMAT_4_15";
		case RESP_INTERNAL_SERVER_ERROR_5_00:
			return "RESP_INTERNAL_SERVER_ERROR_5_00";
		case RESP_NOT_IMPLEMENTED_5_01:
			return "RESP_NOT_IMPLEMENTED_5_01";
		case RESP_BAD_GATEWAY_5_02:
			return "RESP_BAD_GATEWAY_5_02";
		case RESP_SERVICE_UNAVAILABLE_5_03:
			return "RESP_SERVICE_UNAVAILABLE_5_03";
		case RESP_GATEWAY_TIMEOUT_5_04:
			return "RESP_GATEWAY_TIMEOUT_5_04";
		case RESP_PROXYING_NOT_SUPPORTED_5_05:
			return "RESP_PROXYING_NOT_SUPPORTED_5_05";
		default:
			return "UNKNOWN";
	}
}

void _rom CoAP_PrintResultValue(CoAP_Result_t res) {
	if (res == COAP_OK) {
		INFO("COAP_OK\r\n");
	}
	else if (res == COAP_PARSE_DATAGRAM_TOO_SHORT) {
		INFO("COAP_PARSE_DATAGRAM_TOO_SHORT\r\n");
	}
	else if (res == COAP_PARSE_UNKOWN_COAP_VERSION) {
		INFO("COAP_PARSE_UNKOWN_COAP_VERSION\r\n");
	}
	else if (res == COAP_PARSE_MESSAGE_FORMAT_ERROR) {
		INFO("COAP_PARSE_MESSAGE_FORMAT_ERROR\r\n");
	}
	else if (res == COAP_PARSE_TOO_MANY_OPTIONS) {
		INFO("COAP_PARSE_TOO_MANY_OPTIONS\r\n");
	}
	else if (res == COAP_PARSE_TOO_LONG_OPTION) {
		INFO("COAP_PARSE_TOO_LONG_OPTION\r\n");
	}
	else if (res == COAP_PARSE_TOO_MUCH_PAYLOAD) {
		INFO("COAP_PARSE_TOO_MUCH_PAYLOAD\r\n");
	}
	else if (res == COAP_ERR_OUT_OF_MEMORY) {
		INFO("COAP_ERR_OUT_OF_MEMORY\r\n");
	}
	else {
		INFO("UNKNOWN RESULT\r\n");
	}
}

bool _rom CoAP_CharIsPrintable(char c) {
	// according to: https://en.wikipedia.org/wiki/ASCII#Printable_characters
	return c >= 0x20 && c <= 0x7e;
}

char _rom CoAP_CharPrintable(char c) {
	if (CoAP_CharIsPrintable(c)) {
		return c;
	} else {
		return '.';
	}
}
