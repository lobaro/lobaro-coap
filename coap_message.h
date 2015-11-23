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

#ifndef COAP_MESSAGE_H_
#define COAP_MESSAGE_H_

#include "coap_options.h"

//##########################
//### CoAP Message Types ###
//##########################
typedef enum
{
	CON = 0, 	//Confirmable Message
	NON = 1, 	//Non-confirmable Message
	ACK = 2, 	//Acknowlegment Message
	RST = 3 	//Reset Message
}CoAP_MessageType_t;
//typedef uint8_t CoAP_MessageType_t;
//##########################

//##########################
//### CoAP Message Codes ###
//##########################
#define CODE(CLASS, CODE) ( (CLASS <<5) | CODE )
typedef enum
{
	EMPTY 		= CODE(0,0),
	REQ_GET 	= CODE(0,1),
	REQ_POST 	= CODE(0,2),
	REQ_PUT 	= CODE(0,3),
	REQ_DELETE 	= CODE(0,4),
	RESP_SUCCESS_CREATED_2_01 = CODE(2,1), //only used on response to "POST" and "PUT" like HTTP 201
	RESP_SUCCESS_DELETED_2_02 = CODE(2,2), //only used on response to "DELETE" and "POST" like HTTP 204
	RESP_SUCCESS_VALID_2_03 = CODE(2,3),
	RESP_SUCCESS_CHANGED_2_04 = CODE(2,4), //only used on response to "POST" and "PUT" like HTTP 204
	RESP_SUCCESS_CONTENT_2_05 = CODE(2,5), //only used on response to "GET" like HTTP 200 (OK)
	RESP_ERROR_BAD_REQUEST_4_00 = CODE(4,0), //like HTTP 400 (OK)
	RESP_ERROR_UNAUTHORIZED_4_01 = CODE(4,1),
	RESP_BAD_OPTION_4_02 = CODE(4,2),
	RESP_FORBIDDEN_4_03 = CODE(4,3),
	RESP_NOT_FOUND_4_04 = CODE(4,4),
	RESP_METHOD_NOT_ALLOWED_4_05 = CODE(4,5),
	RESP_METHOD_NOT_ACCEPTABLE_4_06 = CODE(4,6),
	RESP_PRECONDITION_FAILED_4_12 = CODE(4,12),
	RESP_REQUEST_ENTITY_TOO_LARGE_4_13 = CODE(4,13),
	RESP_UNSUPPORTED_CONTENT_FORMAT_4_15 = CODE(4,15),
	RESP_INTERNAL_SERVER_ERROR_5_00 = CODE(5,0),
	RESP_NOT_IMPLEMENTED_5_01 = CODE(5,1),
	RESP_BAD_GATEWAY_5_02 = CODE(5,2),
	RESP_SERVICE_UNAVAILABLE_5_03 = CODE(5,3),
	RESP_GATEWAY_TIMEOUT_5_04 = CODE(5,4),
	RESP_PROXYING_NOT_SUPPORTED_5_05 = CODE(5,5)
}CoAP_MessageCode_t;

//##########################
//###    CoAP Message 	 ###
//##########################
typedef struct
{
	uint32_t Timestamp; //set by parse/send network routines
	//VER is implicit = 1
	//TKL (Token Length) is calculated dynamically
	CoAP_MessageType_t Type;					//[1] T
	CoAP_MessageCode_t Code;					//[1] Code
	uint16_t MessageID;							//[2] Message ID (maps ACK msg to coresponding CON msg)
	uint16_t PayloadLength;						//[2]
	uint16_t PayloadBufSize;					//[2] size of allocated msg payload buffer
	uint64_t Token64;							//[8] Token (actual send bytes depend on value inside, e.g. Token=0xfa -> only 1 Byte send!)
	CoAP_option_t* pOptionsList;				//[4] linked list of Options
	uint8_t* Payload;							//[4] MUST be last in struct! Because of mem allocation scheme which tries to allocate message mem and payload mem in ONE big data chunk
}CoAP_Message_t; //total of 24 Bytes
//##########################

#define TOKEN_BYTE(num, token) (((uint8_t*)(&token))[num])
#define TOKEN2STR(token) TOKEN_BYTE(0,token), \
		TOKEN_BYTE(1,token), \
		TOKEN_BYTE(2,token), \
		TOKEN_BYTE(3,token), \
		TOKEN_BYTE(4,token), \
		TOKEN_BYTE(5,token), \
		TOKEN_BYTE(6,token), \
		TOKEN_BYTE(7,token)

CoAP_Message_t* CoAP_CreateMessage(CoAP_MessageType_t Type, CoAP_MessageCode_t Code,
		uint16_t MessageID, uint8_t* pPayloadInitialContent, uint16_t PayloadInitialContentLength, uint16_t PayloadMaxSize, uint64_t Token);

CoAP_Result_t CoAP_ParseMessageFromDatagram(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t** rxedMsg);

CoAP_Result_t CoAP_SendMsg(CoAP_Message_t* Msg, uint8_t ifID, NetEp_t* Receiver);
CoAP_Result_t CoAP_SendEmptyAck(uint16_t MessageID, uint8_t ifID, NetEp_t* Receiver);
CoAP_Result_t CoAP_SendEmptyRST(uint16_t MessageID, uint8_t ifID, NetEp_t* Receiver);
CoAP_Result_t CoAP_SendShortResp(CoAP_MessageType_t Type, CoAP_MessageCode_t Code, uint16_t MessageID, uint64_t token, uint8_t ifID, NetEp_t* Receiver);
CoAP_Message_t* CoAP_AllocRespMsg(CoAP_Message_t* ReqMsg, uint8_t Code, uint16_t PayloadMaxSize);

CoAP_Result_t CoAP_free_Message(CoAP_Message_t** Msg);
void CoAP_free_MsgPayload(CoAP_Message_t** Msg);

bool CoAP_MsgIsRequest(CoAP_Message_t* pMsg);
bool CoAP_MsgIsResponse(CoAP_Message_t* pMsg);
bool CoAP_MsgIsOlderThan(CoAP_Message_t* pMsg, uint32_t timespan);

void CoAP_PrintMsg(CoAP_Message_t* msg);
int CoAP_GetRawSizeOfMessage(CoAP_Message_t* Msg);
void CoAP_PrintResultValue(CoAP_Result_t res);

//note: payload should be set normally by CoAP_SetPayload(...) function, because this function performs
//beside setting the payload also the blockwise transfer logic!
CoAP_Result_t CoAP_addTextPayload(CoAP_Message_t* Msg, char* PayloadStr);
CoAP_Result_t CoAP_addNewPayloadToMessage(CoAP_Message_t* Msg, uint8_t* pData, uint16_t size);

uint16_t CoAP_GetNextMid();
uint64_t CoAP_GenerateToken();

#endif /* COAP_MESSAGE_H_ */
