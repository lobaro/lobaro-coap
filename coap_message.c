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

static uint8_t _rom getTokenByteCount(uint64_t Token){
	if(Token == 0) return 0;
	else if(Token <= 0xff) return 1;
	else if(Token <= 0xffff) return 2;
	else if(Token <= 0xffffff) return 3;
	else if(Token <= 0xffffffff) return 4;
	else if(Token <= 0xffffffffff) return 5;
	else if(Token <= 0xffffffffffff) return 6;
	else if(Token <= 0xffffffffffffff) return 7;
	else return 8;
}

static void _rom CoAP_InitToEmptyResetMsg(CoAP_Message_t* msg)
{
	msg->Type				= RST;
	msg->Code				= EMPTY;
	msg->PayloadLength 		= 0;
	msg->PayloadBufSize		= 0;
	msg->MessageID 			= 0;
	msg->pOptionsList 		= NULL;
	msg->Payload			= NULL;
	msg->Token64 			= 0;
	msg->Timestamp 			= 0;
}

//Checks location of "buf" relative to via bget allocated pMsg buffer
//"CoAP_ParseMessageFromDatagram(...)" puts payload directly within the pMsg buffer to save number of mem allocations
//During lifetime of msg the pointer to payload buffers could move to other locations
//this function checks if buf is part of pMsg (which total alloc size is known)
//returns "false" if buf is external to pMsg else "true"
static bool _rom bufferIsPartOfMsg(uint8_t* buf, CoAP_Message_t* pMsg)
{
	assert( buf!=NULL && pMsg!=NULL);
	//case 1: buffer before Msg Mem
	//case 2: buffer after Msg Mem
	if( (buf < (uint8_t*)(pMsg)) || (buf > ((uint8_t*)(pMsg))+bsize((uint8_t*)pMsg))) return false; //bsize gives total alloc size of msg (user data only)
	else return true;
}

 void _rom CoAP_free_MsgPayload(CoAP_Message_t** Msg)
{
	 if((*Msg)->Payload == NULL ) return;

	//Payload is not under memory allocator control! -> nothing to free here
	if( (*Msg)->Payload > coap_mem_buf_highEnd() || (*Msg)->Payload <coap_mem_buf_lowEnd()) {
		return;
	}

	if(bufferIsPartOfMsg(((*Msg)->Payload), *Msg) == false)
	{
		coap_mem_release((void*) (*Msg)->Payload);
		(*Msg)->Payload = NULL;
		(*Msg)->PayloadBufSize = 0;
	}
	//else will be freed together with Message
}

bool _rom CoAP_MsgIsRequest(CoAP_Message_t* pMsg) {
	  if(pMsg->Code != EMPTY && pMsg->Code <= REQ_DELETE) return true;
	  return false;
}

bool _rom CoAP_MsgIsResponse(CoAP_Message_t* pMsg)
{
	  if(pMsg->Code != EMPTY && pMsg->Code >= RESP_SUCCESS_CREATED_2_01) return true;
	  return false;
}

bool _rom CoAP_MsgIsOlderThan(CoAP_Message_t* pMsg, uint32_t timespan){
	if(hal_rtc_1Hz_Cnt() - pMsg->Timestamp > timespan) return true;
	else return false;
}



CoAP_Result_t _rom CoAP_free_Message(CoAP_Message_t** Msg)
{
	if(*Msg == NULL){
		return COAP_OK; //nothing to free
	}

	if((*Msg)->Type == CON){INFO("- Message memory freed! (CON, MID: %d):\r\n", (*Msg)->MessageID);}
	else if((*Msg)->Type == NON){INFO("- Message memory freed! (NON, MID: %d):\r\n", (*Msg)->MessageID);}
	else if((*Msg)->Type == ACK){INFO("- Message memory freed! (ACK, MID: %d):\r\n", (*Msg)->MessageID);}
	else if((*Msg)->Type == RST){INFO("- Message memory freed! (RST, MID: %d):\r\n", (*Msg)->MessageID);}

	CoAP_FreeOptionList(&((*Msg)->pOptionsList));
	CoAP_free_MsgPayload(Msg);

    //finally delete msg body
	coap_mem_release((void*)(*Msg));
	*Msg = NULL;

	return COAP_OK;
}

static CoAP_MessageType_t _rom CoAP_getRespMsgType(CoAP_Message_t* ReqMsg) { //todo inline it
	if(ReqMsg->Type == CON) return ACK; //for piggybacked responses
	else return NON;
}

static uint16_t _rom CoAP_getRespMsgID(CoAP_Message_t* ReqMsg)
{
	if(ReqMsg->Type == CON) return ReqMsg->MessageID; //for piggybacked responses
	else return CoAP_GetNextMid();
}

CoAP_Message_t* _rom CoAP_AllocRespMsg(CoAP_Message_t* ReqMsg, uint8_t Code, uint16_t PayloadMaxSize) {
	return CoAP_CreateMessage( CoAP_getRespMsgType(ReqMsg), Code, CoAP_getRespMsgID(ReqMsg), NULL, 0, PayloadMaxSize, ReqMsg->Token64);
}


CoAP_Message_t* _rom CoAP_CreateMessage(CoAP_MessageType_t Type, CoAP_MessageCode_t Code,
		uint16_t MessageID, uint8_t* pPayloadInitialContent, uint16_t PayloadInitialContentLength, uint16_t PayloadMaxSize, uint64_t Token)
{
	CoAP_Message_t* pMsg =  (CoAP_Message_t*) coap_mem_get0(sizeof(CoAP_Message_t)+PayloadMaxSize); //malloc space
	if(pMsg == NULL) return NULL;

	//safety checks
	if(PayloadInitialContentLength > PayloadMaxSize) {ERROR("Initial Content bigger than field size!"); return NULL;}

	CoAP_InitToEmptyResetMsg(pMsg); //init

	pMsg->Type = Type;
	pMsg->Code = Code;
	pMsg->MessageID = MessageID;
	pMsg->Token64 = Token;
	pMsg->Timestamp = 0;

	pMsg->PayloadLength 		= PayloadInitialContentLength;
	if(PayloadMaxSize)
	{
		pMsg->PayloadBufSize		= PayloadMaxSize;
		pMsg->Payload				= ((uint8_t*)(pMsg))+sizeof(CoAP_Message_t); //set pointer
		if(pPayloadInitialContent!=NULL) coap_memcpy((void*)((pMsg)->Payload), (void*)pPayloadInitialContent, PayloadInitialContentLength);
	}

	return pMsg;
}

CoAP_Result_t _rom CoAP_ParseMessageFromDatagram(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t** rxedMsg)
{
	//we use local mem and copy afterwards because we dont know yet the size of payload buffer
	//but want to allocate one block for complete final "rxedMsg" memory without realloc the buf size later.
	static CoAP_Message_t Msg;

	uint8_t TokenLength = 0;

	*rxedMsg = NULL;

	if(srcArrLength < 4) return COAP_PARSE_DATAGRAM_TOO_SHORT; // Minimum Size of CoAP Message = 4 Bytes

	CoAP_InitToEmptyResetMsg(&Msg);

//1st Header Byte
	uint8_t Version = srcArr[0] >> 6;
	if(Version != COAP_VERSION) return COAP_PARSE_UNKOWN_COAP_VERSION;

	Msg.Type = (srcArr[0] & 0b110000) >> 4;
	TokenLength = srcArr[0] & 0b1111;
	if(TokenLength > 8)	{INFO("CoAP-Parse Byte1 Error\r\n");return COAP_PARSE_MESSAGE_FORMAT_ERROR;}// return COAP_PARSE_MESSAGE_FORMAT_ERROR;

//2nd & 3rd Header Byte
	Msg.Code = srcArr[1];

	//"Hack" to support early version of "myCoAP" iOS app which sends malformed "CoAP-pings" containing a token...
	//if(Msg.Code == EMPTY && (TokenLength != 0 || srcArrLength != 4))	{INFO("err2\r\n");return COAP_PARSE_MESSAGE_FORMAT_ERROR;}// return COAP_PARSE_MESSAGE_FORMAT_ERROR;

	uint8_t codeClass = ((uint8_t)Msg.Code) >> 5;
	if(codeClass == 1 || codeClass == 6 || codeClass == 7)	{INFO("CoAP-Parse Byte2/3 Error\r\n");return COAP_PARSE_MESSAGE_FORMAT_ERROR;}//  return COAP_PARSE_MESSAGE_FORMAT_ERROR; //reserved classes

//4th Header Byte
	Msg.MessageID = (uint16_t)srcArr[2] << 8 | srcArr[3];


//further parsing locations depend on parsed 4Byte CoAP Header -> use of offset addressing
	uint16_t offset = 4;
	if(srcArrLength == offset) //no more data -> maybe a CoAP Ping
	{
		goto START_MSG_COPY_LABEL; //quick end of parsing...
	}

//Token (if any)
	Msg.Token64 = 0;
	int i;
	for(i=0; i< TokenLength; i++){
		Msg.Token64 |= ((uint64_t)(srcArr[offset+i])) << (8*i);
	}

	offset += TokenLength;
	if(srcArrLength == offset) goto START_MSG_COPY_LABEL;

//Options (if any)
	uint8_t* pPayloadBegin = NULL;

	//this allocates memory for every option and puts it in die pOptionsList linked list
	//start address of payload also given back
	CoAP_Result_t ParseOptionsResult = parse_OptionsFromRaw(&(srcArr[offset]), srcArrLength-offset, &pPayloadBegin, &(Msg.pOptionsList));

	if(ParseOptionsResult != COAP_OK) {
		CoAP_FreeOptionList(&(Msg.pOptionsList));
		return ParseOptionsResult;
	}

//Payload (if any)
	if(pPayloadBegin != NULL)
	{
		Msg.PayloadLength = srcArrLength - (pPayloadBegin - srcArr);
		if(Msg.PayloadLength > MAX_PAYLOAD_SIZE)
		{
			CoAP_FreeOptionList(&(Msg.pOptionsList));
			return COAP_PARSE_TOO_MUCH_PAYLOAD;
		}
	}
	else Msg.PayloadLength = 0;

	Msg.PayloadBufSize = Msg.PayloadLength;

//Get memory for total message data and copy parsed data
//Payload Buffers MUST located at end of CoAP_Message_t to let this work!
START_MSG_COPY_LABEL:
	*rxedMsg = (CoAP_Message_t*) coap_mem_get(sizeof(CoAP_Message_t) + Msg.PayloadLength);

	if(*rxedMsg== NULL)//out of memory
	{
		CoAP_FreeOptionList(&(Msg.pOptionsList));
		return COAP_ERR_OUT_OF_MEMORY;
	}

	coap_memcpy((void*)(*rxedMsg), (void*)&Msg, sizeof(CoAP_Message_t));

	if(Msg.PayloadLength)
	{
		(*rxedMsg)->Payload				= ((uint8_t*)(*rxedMsg))+sizeof(CoAP_Message_t);
		coap_memcpy((void*)((*rxedMsg)->Payload), (void*)pPayloadBegin, Msg.PayloadLength);
	}

	(*rxedMsg)->Timestamp = hal_rtc_1Hz_Cnt();

	return COAP_OK;
}

int CoAP_GetRawSizeOfMessage(CoAP_Message_t* Msg) {
	int TotalMsgBytes = 0;

	TotalMsgBytes+=4; //Header

	TotalMsgBytes+= CoAP_NeededMem4PackOptions(Msg->pOptionsList);

	if(Msg->Code != EMPTY) {

		if(Msg->PayloadLength) {
			TotalMsgBytes+=Msg->PayloadLength+1; //+1 = PayloadMarker
		}

		TotalMsgBytes+=getTokenByteCount(Msg->Token64);
	}

	return TotalMsgBytes;
}

static CoAP_Result_t _rom CoAP_BuildDatagram(uint8_t* destArr, uint16_t* pDestArrSize, CoAP_Message_t* Msg)
{
	uint16_t offset=0;
	uint8_t TokenLength;

	if(Msg->Code == EMPTY) {//must send only 4 byte header overwrite upper layer in any case!
		Msg->PayloadLength = 0;
		TokenLength = 0;
	}else {
		TokenLength = getTokenByteCount(Msg->Token64);
	}

// 4Byte Header (see p.16 RFC7252)
	destArr[0] = 0;
	destArr[0] |= (COAP_VERSION & 3) << 6;
	destArr[0] |= (Msg->Type & 3) << 4;
	destArr[0] |= (TokenLength & 15);
	destArr[1] = (uint8_t)Msg->Code;
	destArr[2] = (uint8_t)(Msg->MessageID >> 8);
	destArr[3] = (uint8_t)(Msg->MessageID & 0xff);

	offset += 4;

// Token (0 to 8 Bytes)
	int i;
	for(i=0; i< TokenLength; i++)
	{
		destArr[offset+i] = ((uint8_t)((Msg->Token64)>>8*i)) & 0xff;
	}
	offset += TokenLength;

// Options
	if(Msg->pOptionsList != NULL)
	{
		uint16_t OptionsRawByteCount = 0;
		//iterates through (ascending sorted!) list of options and encodes them in CoAPs compact binary representation
		pack_OptionsFromList(&(destArr[offset]), &OptionsRawByteCount, Msg->pOptionsList);

		offset += OptionsRawByteCount;
	}

//Payload
	if(Msg->PayloadLength != 0)
	{
			destArr[offset] = 0xff; //Payload Marker
			offset++;

			coap_memcpy((void*)&(destArr[offset]), (void*)(Msg->Payload), Msg->PayloadLength);

			offset+=Msg->PayloadLength;
	}

	*pDestArrSize = offset; // => Size of Datagram array
	return COAP_OK;
}


//send minimal 4Byte header CoAP empty ACK message
CoAP_Result_t _rom CoAP_SendEmptyAck(uint16_t MessageID, uint8_t ifID, NetEp_t* Receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)

	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = ACK;
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, ifID, Receiver);
}

//send short response
CoAP_Result_t _rom CoAP_SendShortResp(CoAP_MessageType_t Type, CoAP_MessageCode_t Code, uint16_t MessageID, uint64_t token, uint8_t ifID, NetEp_t* Receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = Type;
	Msg.MessageID = MessageID;
	Msg.Code = Code;
	Msg.Token64 = token;
	return CoAP_SendMsg(&Msg, ifID, Receiver);
}

//send minimal 4Byte header CoAP empty RST message
CoAP_Result_t _rom CoAP_SendEmptyRST(uint16_t MessageID, uint8_t ifID, NetEp_t* Receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, ifID, Receiver);
}

CoAP_Result_t _rom CoAP_SendMsg(CoAP_Message_t* Msg, uint8_t ifID, NetEp_t* Receiver)
{
	int i;
	uint16_t bytesToSend = 0;
	CoAP_Result_t res;
	NetSocket_t* pSocket = RetrieveSocket2(ifID);
	NetTransmit_fn SendPacket = pSocket->Tx;
	uint8_t quickBuf[16]; //speed up sending of tiny messages

	if(SendPacket == NULL) {
		ERROR("SendPacket function not found! idID: %d", ifID);
		return COAP_NOT_FOUND;
	}

//build generic packet
	NetPacket_t pked;
	pked.Receiver = *Receiver;

//get the socket on which request has been received
	pked.Sender.NetType = pSocket->EpLocal.NetType;
	pked.Sender.NetAddr = pSocket->EpLocal.NetAddr;
	pked.Sender.NetPort = pSocket->EpLocal.NetPort;

//Alloc raw memory
	pked.size = CoAP_GetRawSizeOfMessage(Msg);
	if(pked.size<=16) { //for small messages don't take overhead of mem allocation
		pked.pData = quickBuf;
	}else {
		pked.pData = (uint8_t*) coap_mem_get(pked.size);
		if(pked.pData == NULL) return COAP_ERR_OUT_OF_MEMORY;
	}

//serialize msg
	CoAP_BuildDatagram((pked.pData), &bytesToSend, Msg);

	if(bytesToSend!=pked.size){
		INFO("(!!!) Bytes to Send = %d estimated = %d\r\n", bytesToSend, CoAP_GetRawSizeOfMessage(Msg));
	}


	INFO("\r\n>>>>>>>>>>>>>>>>>>>>>>\r\nSend Message [%d Bytes], Interface #%u\r\n", bytesToSend, ifID);
	INFO("Receiving Endpoint: ");
	PrintEndpoint(&(pked.Receiver));

	for(i=0; i<pked.size; i++)
	{
		if(pked.pData[i]!=0){ //0 = string end
			INFO("0x%02x(%c) ", pked.pData[i], pked.pData[i]);
		}else{
			INFO("0x00() ");
		}
	}
	INFO("\r\n");

	if( SendPacket(ifID, &pked) == true ) { // send COAP_OK!
		Msg->Timestamp = hal_rtc_1Hz_Cnt();
		CoAP_PrintMsg(Msg);
		INFO(">>>>>>>>>>OK>>>>>>>>>>\r\n");
		if(pked.pData != quickBuf) { coap_mem_release(pked.pData); }
		return COAP_OK;
	} else {
		CoAP_PrintMsg(Msg);
		INFO(">>>>>>>>>>FAIL>>>>>>>>>>\r\n");
		if(pked.pData != quickBuf) { coap_mem_release(pked.pData); }
		return COAP_ERR_NETWORK;
	}

}

//todo use random and hash
uint16_t _rom CoAP_GetNextMid(){
	static uint16_t MId = 0;
	MId++;
	return MId;
}

uint64_t _rom CoAP_GenerateToken() {
	static uint64_t Token = 0xfa;
	Token++;
	return Token;
}

CoAP_Result_t _rom CoAP_addNewPayloadToMessage(CoAP_Message_t* Msg, uint8_t* pData, uint16_t size)
{
	if(size > MAX_PAYLOAD_SIZE) {
		ERROR("payload > MAX_PAYLOAD_SIZE");
		return COAP_ERR_OUT_OF_MEMORY;
	}

	if(size)
	{
		if(Msg->PayloadBufSize >= size){
			coap_memcpy(Msg->Payload, pData, size); //use existing buffer
		}
		else { // will move payload buf outside of msg memory frame!
			CoAP_free_MsgPayload(&Msg); //free old buffer
			Msg->Payload = (uint8_t*)coap_mem_get(size); //alloc a different new buffer
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
	return CoAP_addNewPayloadToMessage(Msg, (uint8_t*)PayloadStr, (uint16_t)(strlen(PayloadStr)));
}

void _rom CoAP_PrintMsg(CoAP_Message_t* msg) {
	INFO("----------------------------\r\n");

	if(msg->Type == CON){LOG_DEBUG("*Type: CON (0x%02x)\r\n", msg->Type);}
	else if(msg->Type == NON){LOG_DEBUG("*Type: NON (0x%02x)\r\n", msg->Type);}
	else if(msg->Type == ACK){LOG_DEBUG("*Type: ACK (0x%02x)\r\n", msg->Type);}
	else if(msg->Type == RST){LOG_DEBUG("*Type: RST (0x%02x)\r\n", msg->Type);}
	else {LOG_DEBUG("*Type: UNKNOWN! (0x%02x)\r\n", msg->Type);}

	uint8_t tokenBytes = getTokenByteCount(msg->Token64);
	if(tokenBytes > 0) {
		LOG_DEBUG("*Token: %u Byte -> 0x",tokenBytes);
		int i;
		for(i=0; i<tokenBytes;i++) {
			LOG_DEBUG("%02x",TOKEN_BYTE(i,msg->Token64));
		}
	} else {
		LOG_DEBUG("*Token: %u Byte -> 0",tokenBytes);
	}

	uint8_t code = msg->Code;
	LOG_DEBUG("\r\n*Code: %d.%02d (0x%02x) ", code >> 5, code & 31, code);

	if(msg->Code == EMPTY){LOG_DEBUG("[EMPTY]\r\n");}
	else if(msg->Code == REQ_GET){LOG_DEBUG("[REQ_GET]\r\n");}
	else if(msg->Code == REQ_POST){LOG_DEBUG("[REQ_POST]\r\n");}
	else if(msg->Code == REQ_PUT){LOG_DEBUG("[REQ_PUT]\r\n");}
	else if(msg->Code == REQ_DELETE){LOG_DEBUG("[REQ_DELETE]\r\n");}
	else LOG_DEBUG("\r\n");

	LOG_DEBUG("*MessageId: %u\r\n", msg->MessageID);

	CoAP_option_t* pOption = NULL;
	if(msg->pOptionsList != NULL)
	{
		pOption = msg->pOptionsList;
	}
	while(pOption != NULL)
	{
		INFO("*Option #%u (Length=%u) ->", pOption->Number, pOption->Length);
		int j;
		for(j=0; j< pOption->Length; j++){
			if(pOption->Value[j]) {
			INFO(" %c[", pOption->Value[j]);
			INFO("%02x]", pOption->Value[j]);
			}else {
				INFO("  [00]", pOption->Value[j]);
			}
		}
		INFO("\r\n");
		pOption = pOption->next;
	}
	if(msg->PayloadLength) {
		LOG_DEBUG("*Payload (%u Byte): \"", msg->PayloadLength);
		if(msg->PayloadLength > MAX_PAYLOAD_SIZE){LOG_DEBUG("too much payload!");}
		else {
			int i;
			for(i=0; i< msg->PayloadLength && i < MAX_PAYLOAD_SIZE; i++){LOG_DEBUG("%c",msg->Payload[i]);}
		}
		LOG_DEBUG("\"\r\n");
	}

	INFO("*Timestamp: %d\r\n", msg->Timestamp);
	INFO("----------------------------\r\n");
}

void _rom CoAP_PrintResultValue(CoAP_Result_t res)
{
	if(res == COAP_OK) { INFO("COAP_OK\r\n"); }
	else if(res == COAP_PARSE_DATAGRAM_TOO_SHORT) { INFO("COAP_PARSE_DATAGRAM_TOO_SHORT\r\n"); }
	else if(res == COAP_PARSE_UNKOWN_COAP_VERSION) { INFO("COAP_PARSE_UNKOWN_COAP_VERSION\r\n"); }
	else if(res == COAP_PARSE_MESSAGE_FORMAT_ERROR) { INFO("COAP_PARSE_MESSAGE_FORMAT_ERROR\r\n"); }
	else if(res == COAP_PARSE_TOO_MANY_OPTIONS) { INFO("COAP_PARSE_TOO_MANY_OPTIONS\r\n"); }
	else if(res == COAP_PARSE_TOO_LONG_OPTION) { INFO("COAP_PARSE_TOO_LONG_OPTION\r\n"); }
	else if(res == COAP_PARSE_TOO_MUCH_PAYLOAD) { INFO("COAP_PARSE_TOO_MUCH_PAYLOAD\r\n"); }
	else if(res == COAP_ERR_OUT_OF_MEMORY) { INFO("COAP_ERR_OUT_OF_MEMORY\r\n"); }
	else  { INFO("UNKNOWN RESULT\r\n"); }
}
