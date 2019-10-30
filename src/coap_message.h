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
#include "liblobaro_coap.h"

#define TOKEN_BYTE(num, token) (((uint8_t*)(&token))[num])
#define TOKEN2STR(token) TOKEN_BYTE(0,token), \
		TOKEN_BYTE(1,token), \
		TOKEN_BYTE(2,token), \
		TOKEN_BYTE(3,token), \
		TOKEN_BYTE(4,token), \
		TOKEN_BYTE(5,token), \
		TOKEN_BYTE(6,token), \
		TOKEN_BYTE(7,token)

bool CoAP_TokenEqual(CoAP_Token_t a, CoAP_Token_t b);

CoAP_Message_t* CoAP_CreateMessage(CoAP_MessageType_t Type, CoAP_MessageCode_t Code,
		uint16_t MessageID, uint8_t* pPayloadInitialContent, uint16_t PayloadInitialContentLength, uint16_t PayloadMaxSize, CoAP_Token_t Token);

CoAP_Result_t CoAP_ParseMessageFromDatagram(uint8_t* srcArr, uint16_t srcArrLength, CoAP_Message_t** rxedMsg);

CoAP_Result_t CoAP_SendMsg(CoAP_Message_t* Msg, SocketHandle_t socketHandle, NetEp_t receiver);
CoAP_Result_t CoAP_SendEmptyAck(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver);
CoAP_Result_t CoAP_SendEmptyRST(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver);
CoAP_Result_t CoAP_SendShortResp(CoAP_MessageType_t Type, CoAP_MessageCode_t Code, uint16_t MessageID, CoAP_Token_t token, SocketHandle_t socketHandle, NetEp_t receiver);
CoAP_Message_t* CoAP_AllocRespMsg(CoAP_Message_t* ReqMsg, CoAP_MessageCode_t Code, uint16_t PayloadMaxSize);

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

void CoAP_InitIds();
uint16_t CoAP_GetNextMid();
CoAP_Token_t CoAP_GenerateToken();

#endif /* COAP_MESSAGE_H_ */
