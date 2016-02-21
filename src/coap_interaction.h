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
#ifndef COAP_INTERACTIONS_H
#define COAP_INTERACTIONS_H

typedef enum
{
	COAP_ROLE_NOT_SET,
	COAP_ROLE_SERVER,		//[server]
	COAP_ROLE_NOTIFICATION,	//[notificator]
	COAP_ROLE_CLIENT		//[client]
}CoAP_InteractionRole_t;

typedef enum
{
	COAP_STATE_NOT_SET,							// set after memory allocation

	//[server]
	COAP_STATE_HANDLE_REQUEST, 					// Remote request received & parsed -> invoke resource handler
	COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT,// Wait some time for resource to become ready, meanwhile empty ack has been sent
	COAP_STATE_RESPONSE_SENT,					// NON or piggy-ACK response has been sent -> wait some time then delete
												// CON response has been sent (+previous separate ACK of req)
												// -> wait for ACK or resent after some time

	//[notificator]
	COAP_STATE_READY_TO_NOTIFY,					// Notification is ready to be send (triggered by resource event)
	COAP_STATE_NOTIFICATION_SENT,				// NON response has been sent -> wait some time then delete
												// CON response has been sent -> wait for ACK or resent after some time

	//[client]
	COAP_STATE_READY_TO_REQUEST,				// request is ready to send
	COAP_STATE_WAITING_RESPONSE,				// resend request after some time if no response or ack has been received for some time
	COAP_STATE_HANDLE_RESPONSE,					// call client callback and delete after some time, ack maybe send too

	//[server]+[notificator]+[client]
	COAP_STATE_FINISHED							// can be deleted / freed
}CoAP_InteractionState_t;

typedef enum {
	NOT_SET,
	ACK_SET,
	RST_SET
}CoAP_ReliabilityState_t;

typedef  CoAP_Result_t ( * CoAP_RespHandler_fn_t ) (CoAP_Message_t* pRespMsg, NetEp_t* Sender);

struct CoAP_Interaction
{
	struct CoAP_Interaction* next; 					//4 byte pointer (linked list of interactions)

	CoAP_InteractionRole_t Role; 					//[client], [server] or [notification]
	CoAP_InteractionState_t State;

	CoAP_Res_t* pRes; 								//Resource of IA
	CoAP_Observer_t* pObserver;						//"NULL" or link to Observer (Role=COAP_ROLE_NOTIFICATION)

	//Network State
	NetEp_t RemoteEp;  								//[server]=>requesting EP, [client]=>serving EP
	uint8_t ifID; 									//Interface ID

	//control vars
	bool 	 UpdatePendingNotification;					//control flag for Observe RFC7641 ("4.5.2.  Advanced Transmission")
	uint8_t  RetransCounter;
	uint32_t AckTimeout;
	uint32_t SleepUntil;

	//Request
	CoAP_Message_t* pReqMsg;
	CoAP_ReliabilityState_t ReqReliabilityState; 	//(unused on NON)
	MetaInfo_t ReqMetaInfo;

	//Response
	CoAP_Message_t* pRespMsg;						 //corresponding Response Message
	CoAP_ReliabilityState_t RespReliabilityState;	 //(unused on NON)
	MetaInfo_t RespMetaInfo;

	CoAP_RespHandler_fn_t RespCB; 					 //response callback (if client)
};

typedef struct CoAP_Interaction CoAP_Interaction_t;

//called by incoming request
CoAP_Result_t CoAP_StartNewServerInteraction(CoAP_Message_t* pMsgReq, CoAP_Res_t* pRes, uint8_t ifID, NetPacket_t* pRawPckt); //we act as a CoAP Server (receiving requests) in this interaction

//called by internal requests to external servers (client mode)
CoAP_Result_t CoAP_StartNewClientInteraction(CoAP_Message_t* pMsgReq, uint8_t ifID, NetEp_t* ServerEp, CoAP_RespHandler_fn_t cb);//we act as a CoAP Client (sending requests) in this interaction
CoAP_Result_t CoAP_StartNewGetRequest(char* UriString, uint8_t ifID, NetEp_t* ServerEp, CoAP_RespHandler_fn_t cb);

CoAP_Result_t CoAP_RemoveInteractionsObserver(CoAP_Interaction_t* pIA, uint64_t token);
CoAP_Result_t CoAP_HandleObservationInReq(CoAP_Interaction_t* pIA);
CoAP_Result_t CoAP_StartNotifyInteractions(CoAP_Res_t* pRes);

CoAP_Result_t CoAP_FreeInteraction(CoAP_Interaction_t** pInteraction);

CoAP_Interaction_t* CoAP_GetLongestPendingInteraction();
CoAP_Result_t CoAP_DeleteInteraction(CoAP_Interaction_t* pInteractionToDelete);
CoAP_Result_t CoAP_ResetInteractionByID(uint16_t MsgID, uint8_t ifID, NetEp_t* RstEp);
CoAP_Result_t CoAP_EnqueueLastInteraction(CoAP_Interaction_t* pInteractionToEnqueue);

CoAP_Result_t CoAP_SetSleepInteraction(CoAP_Interaction_t* pIA, uint32_t seconds);
CoAP_Result_t CoAP_EnableAckTimeout(CoAP_Interaction_t* pIA, uint8_t retryNum);
//client
CoAP_Interaction_t* CoAP_GetInteractionByMessageID(uint16_t mId);

CoAP_Interaction_t* CoAP_ApplyReliabilityStateToInteraction(CoAP_ReliabilityState_t stateToAdd, uint16_t mID, uint8_t ifID, NetEp_t* fromEp);
#endif
