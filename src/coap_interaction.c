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

CoAP_Result_t CoAP_HandleObservationInReq(CoAP_Interaction_t* pIA);

static CoAP_Interaction_t* _rom CoAP_AllocNewInteraction()
{
	CoAP_Interaction_t* newInteraction = (CoAP_Interaction_t*)(coap_mem_get0(sizeof(CoAP_Interaction_t)));
	if(newInteraction == NULL) {
		INFO("- (!!!) CoAP_AllocNewInteraction() Out of Memory!!!\r\n");
		return NULL;
	}

	newInteraction->RetransCounter=0;
	newInteraction->AckTimeout = 0;
	newInteraction->SleepUntil = 0;

	newInteraction->UpdatePendingNotification = false;
	newInteraction->ifID = 0;
	newInteraction->pReqMsg = NULL;
	newInteraction->pRespMsg = NULL;
	newInteraction->pObserver = NULL;
	newInteraction->next = NULL;
	newInteraction->ReqReliabilityState = NOT_SET;
	newInteraction->RespReliabilityState = NOT_SET;
	newInteraction->Role = COAP_ROLE_NOT_SET;
	newInteraction->State = COAP_STATE_NOT_SET;

	return newInteraction;
}

CoAP_Result_t _rom CoAP_FreeInteraction(CoAP_Interaction_t** pInteraction)
{
	INFO("Releasing Interaction...\r\n");
	coap_mem_stats();
	CoAP_free_Message(&(*pInteraction)->pReqMsg);
	CoAP_free_Message(&(*pInteraction)->pRespMsg);
	coap_mem_release((void*)(*pInteraction));
	coap_mem_stats();

	*pInteraction = NULL;
	return COAP_OK;
}


static CoAP_Result_t _rom CoAP_UnlinkInteractionFromList(CoAP_Interaction_t** pListStart, CoAP_Interaction_t* pInteractionToRemove, bool FreeUnlinked)
{
	CoAP_Interaction_t* currP;
	CoAP_Interaction_t* prevP;

  // For 1st node, indicate there is no previous.
  prevP = NULL;

   //Visit each node, maintaining a pointer to
   //the previous node we just visited.
  for (currP = *pListStart; currP != NULL;
		  prevP = currP, currP = currP->next) {

    if (currP == pInteractionToRemove) {  // Found it.
      if (prevP == NULL) {
        //Fix beginning pointer.
        *pListStart = currP->next;
      } else {
        //Fix previous node's next to
        //skip over the removed node.
        prevP->next = currP->next;
      }

      // Deallocate the node.
      if(FreeUnlinked) CoAP_FreeInteraction(&currP);
      //Done searching.
      return COAP_OK;
    }
  }
  return COAP_OK;
}

static CoAP_Result_t _rom CoAP_UnlinkInteractionFromListByID(CoAP_Interaction_t** pListStart, uint16_t MsgID, uint8_t ifID, NetEp_t* RstEp, bool FreeUnlinked)
{
	CoAP_Interaction_t* currP;
	CoAP_Interaction_t* prevP;

  // For 1st node, indicate there is no previous.
  prevP = NULL;

   //Visit each node, maintaining a pointer to
   //the previous node we just visited.
  for (currP = *pListStart; currP != NULL;
		  prevP = currP, currP = currP->next) {

    if (currP->ifID == ifID
    	&& ((currP->pReqMsg && currP->pReqMsg->MessageID == MsgID) || (currP->pRespMsg && currP->pRespMsg->MessageID == MsgID))
    	&& EpAreEqual( &(currP->RemoteEp), RstEp)) {  // Found it.

      if (prevP == NULL) {
        //Fix beginning pointer.
        *pListStart = currP->next;
      } else {
        //Fix previous node's next to
        //skip over the removed node.
        prevP->next = currP->next;
      }

      // Deallocate the node.
      if(FreeUnlinked) CoAP_FreeInteraction(&currP);
      //Done searching.
      return COAP_OK;
    }
  }
  return COAP_NOT_FOUND;
}



static CoAP_Result_t _rom CoAP_AppendInteractionToList(CoAP_Interaction_t** pListStart, CoAP_Interaction_t* pInteractionToAdd)
{
	if(pInteractionToAdd == NULL) return COAP_ERR_ARGUMENT;

	if(*pListStart == NULL) //List empty? create new first element
	{
		*pListStart = pInteractionToAdd;
		(*pListStart)->next = NULL;
	}
	else //append new element at end
	{
		CoAP_Interaction_t*  pTrans = *pListStart;
	    while(pTrans->next != NULL) pTrans=pTrans->next;

	    pTrans->next = pInteractionToAdd;
		pTrans = pTrans->next;
		pTrans->next = NULL;
	}
	return COAP_OK;
}


static CoAP_Result_t _rom CoAP_MoveInteractionToListEnd(CoAP_Interaction_t** pListStart, CoAP_Interaction_t* pInteractionToMove)
{
	CoAP_Interaction_t* currP;
	CoAP_Interaction_t* prevP;

  // For 1st node, indicate there is no previous.
  prevP = NULL;

 //is interaction in List? if so delete it temporarily and add it to the back then

   //Visit each node, maintaining a pointer to
   //the previous node we just visited.
  for (currP = *pListStart;
		  currP != NULL;
		  prevP = currP, currP = currP->next) {

    if (currP == pInteractionToMove) {  // Found it.
      if (prevP == NULL) {
        //Fix beginning pointer.
        *pListStart = currP->next;
      } else {
        //Fix previous node's next to
        //skip over the removed node.
        prevP->next = currP->next;
      }
    }
  }
  //node removed now put it at end of list
  CoAP_AppendInteractionToList(pListStart, pInteractionToMove);

  return COAP_OK;
}


CoAP_Result_t _rom CoAP_SetSleepInteraction(CoAP_Interaction_t* pIA, uint32_t seconds) {
	pIA->SleepUntil = hal_rtc_1Hz_Cnt()+seconds;
	return COAP_OK;
}

CoAP_Result_t _rom CoAP_EnableAckTimeout(CoAP_Interaction_t* pIA, uint8_t retryNum) {
	uint32_t waitTime = ACK_TIMEOUT;
	int i;
	for(i=0; i < retryNum; i++) { //"exponential backoff"
		waitTime*=ACK_TIMEOUT;
	}

	 pIA->AckTimeout = hal_rtc_1Hz_Cnt()+ waitTime;
	return COAP_OK;
}

CoAP_Interaction_t* _rom CoAP_GetLongestPendingInteraction()
{
	return CoAP.pInteractions;
}

CoAP_Interaction_t* _rom CoAP_GetInteractionByMessageID(uint16_t mId)
{
	CoAP_Interaction_t* pList = CoAP.pInteractions;

	if(pList == NULL) return NULL;

	if(pList->pReqMsg != NULL
			&& pList->pReqMsg->MessageID == mId) return pList;


	 while(pList->next != NULL)
	 {
		if(pList->pReqMsg != NULL
			&& pList->pReqMsg->MessageID == mId) return pList;

		 pList=pList->next;
	 }

	return NULL;
}


CoAP_Result_t _rom CoAP_DeleteInteraction(CoAP_Interaction_t* pInteractionToDelete)
{
  return CoAP_UnlinkInteractionFromList(&(CoAP.pInteractions), pInteractionToDelete, true);
}


CoAP_Result_t _rom CoAP_ResetInteractionByID(uint16_t MsgID, uint8_t ifID, NetEp_t* RstEp)
{

	return CoAP_UnlinkInteractionFromListByID(&(CoAP.pInteractions), MsgID, ifID, RstEp, true);
}

//Iterate over all interactions to find corresponding (by mID) message send by us before
//and apply the newly received RST/ACK state
//returns IA found
CoAP_Interaction_t* _rom CoAP_ApplyReliabilityStateToInteraction(CoAP_ReliabilityState_t stateToAdd, uint16_t mID, uint8_t ifID, NetEp_t* fromEp){
	CoAP_Interaction_t* pList = CoAP.pInteractions;

	//A received RST message rejects a former send CON message or (optional) NON message send by us
	//A received ACK message acknowledges a former send CON message or (optional) NON message send by us
	//servers and notificators use CON only in responses, clients in requests
	while(pList != NULL){

		if(pList->Role == COAP_ROLE_SERVER || pList->Role == COAP_ROLE_NOTIFICATION) {
			if(pList->pRespMsg != NULL && pList->pRespMsg->MessageID == mID && EpAreEqual(fromEp, &(pList->RemoteEp))) {
				pList->RespReliabilityState = stateToAdd; //info: observer disconnection is done in statemachine (see coap_main.c)
				return pList;
			}
		}
		else if(pList->Role == COAP_ROLE_CLIENT){
			if(pList->pReqMsg != NULL && pList->pReqMsg->MessageID == mID && EpAreEqual(fromEp, &(pList->RemoteEp))) {
				pList->ReqReliabilityState = stateToAdd;
				return pList;
			}
		}

		pList = pList->next;
	}

	return NULL;
}

CoAP_Result_t _rom CoAP_AckInteractionByID(uint16_t MsgID, uint8_t ifID, NetEp_t* fromEp)
{
//	CoAP_Interaction_t* pIA = CoAP.pInteractions; //List of all interactions
//
//	while(pIA != NULL) { //visit all interactions
//		if(pIA->State == COAP_STATE_WAITING_ACK && pIA->ifID == ifID) {
//			if(pIA->Role == COAP_ROLE_SERVER) {
//
//				if(pIA->pMsgResp != NULL && pIA->pMsgResp->MessageID==MsgID && EpAreEqual(fromEp, pIA->RespEp)) {
//					pIA->RespAcked = true;
//					return;
//				}
//
//			}else if(pIA->Role == COAP_ROLE_CLIENT || pIA->Role == COAP_ROLE_NOTIFICATOR){
//
//
//			}else {
//				INFO("- (!!!) CoAP_AckInteractionByID(): unexpected IA Role!!!\r\n");
//			}
//
//		}
//
//		if(pIA->pMsgReq != NULL) {
//
//		}
//
//
//
//		if(pIA->State == COAP_STATE_WAITING_ACK  && pIA->ifID == ifID) {
//			if(pIA->Role == COAP_ROLE_SERVER) {
//				if(pIA->pMsgResp != NULL && pIA->pMsgResp->MessageID==MsgID && EpAreEqual(fromEp, pIA->ReqEp))
//
//
//			}else if(pIA->Role == COAP_ROLE_CLIENT || pIA->Role == COAP_ROLE_NOTIFICATOR) {
//
//			}
//		}
//
//
//		pIA = pIA->next;
//	}
//
return COAP_NOT_FOUND;
}

CoAP_Result_t _rom CoAP_EnqueueLastInteraction(CoAP_Interaction_t* pInteractionToEnqueue){
	return CoAP_MoveInteractionToListEnd(&(CoAP.pInteractions), pInteractionToEnqueue);
}


//we act as a CoAP Client (sending requests) in this interaction
CoAP_Result_t _rom CoAP_StartNewClientInteraction(CoAP_Message_t* pMsgReq, uint8_t ifID, NetEp_t* ServerEp, CoAP_RespHandler_fn_t cb)
{
	if(pMsgReq==NULL || CoAP_MsgIsRequest(pMsgReq) == false) return COAP_ERR_ARGUMENT;

	CoAP_Interaction_t* newIA = CoAP_AllocNewInteraction();
	if(newIA == NULL) return COAP_ERR_OUT_OF_MEMORY;

	//attach request message
	newIA->pReqMsg = pMsgReq;

	newIA->ifID = ifID;
	CopyEndpoints(&(newIA->RemoteEp), ServerEp);
	newIA->RespCB = cb;

	newIA->Role = COAP_ROLE_CLIENT;
	newIA->State = COAP_STATE_READY_TO_REQUEST;

	CoAP_AppendInteractionToList(&(CoAP.pInteractions), newIA);

	return COAP_OK;
}

CoAP_Result_t _rom CoAP_StartNewGetRequest(char* UriString, uint8_t ifID, NetEp_t* ServerEp, CoAP_RespHandler_fn_t cb) {

	CoAP_Message_t* pReqMsg = CoAP_CreateMessage(CON, REQ_GET, CoAP_GetNextMid(),NULL, 0,0, CoAP_GenerateToken());

	if(pReqMsg != NULL) {
		CoAP_AppendUriOptionsFromString(&(pReqMsg->pOptionsList), UriString);
		return CoAP_StartNewClientInteraction(pReqMsg, ifID, ServerEp, cb);
	}

	INFO("- New GetRequest failed: Out of Memory\r\n");

	return COAP_ERR_OUT_OF_MEMORY;
}

CoAP_Result_t _rom CoAP_StartNotifyInteractions(CoAP_Res_t* pRes) {
	CoAP_Observer_t* pObserver = pRes->pListObservers;
	CoAP_Interaction_t* newIA;
	bool SameNotificationOngoing;

	//iterate over all observers of this resource and check if
	//a) a new notification interaction has to be created
	//or
	//b) wait for currently pending notification to end
	//or
	//c) a currently pending older notification has be updated to the new resource representation - as stated in Observe RFC7641 ("4.5.2.  Advanced Transmission")
	while(pObserver != NULL) {

		//search for pending notification of this resource to this observer and set update flag of representation
	    SameNotificationOngoing=false;
	    CoAP_Interaction_t* pIA;
		for (pIA = CoAP.pInteractions; pIA != NULL; pIA = pIA->next) {
			if(pIA->Role==COAP_ROLE_NOTIFICATION){
				if(pIA->pRes == pRes && pIA->ifID == pObserver->IfID && EpAreEqual( &(pIA->RemoteEp), &(pObserver->Ep))) {
					SameNotificationOngoing = true;

					#if USE_RFC7641_ADVANCED_TRANSMISSION == 1
					pIA->UpdatePendingNotification = true; //will try to update the ongoing resource representation on ongoing transfer
					#endif

					break;
				}
			}
		}

		if(SameNotificationOngoing) {
			pObserver = pObserver->next; //skip this observer, don't start a 2nd notification now
			continue;
		}

		//Start new IA for this observer
		newIA = CoAP_AllocNewInteraction();
		if(newIA == NULL) return COAP_ERR_OUT_OF_MEMORY;

		//Create fresh response message
		newIA->pRespMsg = CoAP_CreateMessage( CON, RESP_SUCCESS_CONTENT_2_05, CoAP_GetNextMid(), NULL, 0, PREFERED_PAYLOAD_SIZE, pObserver->Token);

		//Call Notify Handler of resource and add to interaction list
		if(newIA->pRespMsg != NULL && pRes->Notifier != NULL &&
				pRes->Notifier(pObserver, newIA->pRespMsg) == COAP_OK) { //<------ call notify handler of resource

			newIA->Role = COAP_ROLE_NOTIFICATION;
			newIA->State = COAP_STATE_READY_TO_NOTIFY;
			newIA->RemoteEp = pObserver->Ep;
			newIA->ifID = pObserver->IfID;
			newIA->pRes = pRes;
			newIA->pObserver = pObserver;



			if(newIA->pRespMsg->Code >=RESP_ERROR_BAD_REQUEST_4_00) { //remove this observer from resource in case of non OK Code (see RFC7641, 3.2., 3rd paragraph)
				pObserver = pObserver->next; //next statement will free current observer so save its ancestor node right now
				CoAP_RemoveInteractionsObserver(newIA, newIA->pRespMsg->Token64);
				continue;
			} else {
				AddObserveOptionToMsg(newIA->pRespMsg, pRes->UpdateCnt); // Only 2.xx responses do include an Observe Option.
			}

			if(newIA->pRespMsg->Type == NON && pRes->UpdateCnt % 20 == 0) { //send every 20th message as CON even if notify handler defines the send out as NON to support "lazy" cancelation
				newIA->pRespMsg->Type = CON;
			}


			CoAP_AppendInteractionToList(&(CoAP.pInteractions), newIA);

		}else {
			CoAP_FreeInteraction(&newIA); //revert IA creation above
		}

		pObserver = pObserver->next;
	}
	return COAP_OK;
}

//we act as a CoAP Server (receiving requests) in this interaction
CoAP_Result_t _rom CoAP_StartNewServerInteraction(CoAP_Message_t* pMsgReq, CoAP_Res_t* pRes, uint8_t ifID, NetPacket_t* pRawPckt)
{
	if(!CoAP_MsgIsRequest(pMsgReq)) return COAP_ERR_ARGUMENT;

	//NON or CON request
	NetEp_t* pReqEp = &(pRawPckt->Sender);
	CoAP_Interaction_t*  pIA = CoAP.pInteractions;

	//duplicate detection:
	//same request already received before?
	//iterate over all interactions
	for (pIA = CoAP.pInteractions; pIA != NULL; pIA = pIA->next) {
		if(pIA->Role == COAP_ROLE_SERVER && pIA->ifID == ifID
			&& pIA->pReqMsg->MessageID == pMsgReq->MessageID
			&& EpAreEqual( &(pIA->RemoteEp), pReqEp))
		{
			//implements 4.5. "SHOULD"s
			if(pIA->pReqMsg->Type==CON && pIA->State == COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT) { //=> must be postponed resource with empty ack already sent, send it again
				CoAP_SendEmptyAck(pIA->pReqMsg->MessageID, pIA->ifID, &(pIA->RemoteEp)); //send another empty ack
			}

			//pIA->ReqReliabilityState

			return COAP_ERR_EXISTING;
		}
	 }

	//no duplicate request found-> create a new interaction for this new request
	CoAP_Interaction_t* newIA = CoAP_AllocNewInteraction();

	if(newIA == NULL)
	{
		ERROR("(!) can't create new interaction - out of memory!\r\n");
		return COAP_ERR_OUT_OF_MEMORY;
	}

	newIA->ifID = ifID;
	newIA->RespReliabilityState = NOT_SET;
	newIA->RespCB = NULL;
	newIA->pRes = pRes;
	newIA->Role = COAP_ROLE_SERVER;
	newIA->State = COAP_STATE_HANDLE_REQUEST;
	newIA->ReqMetaInfo = pRawPckt->MetaInfo;
	newIA->ReqReliabilityState = NOT_SET;
	newIA->pReqMsg = pMsgReq;

	CopyEndpoints(&(newIA->RemoteEp), pReqEp);
	CoAP_AppendInteractionToList(&(CoAP.pInteractions), newIA);
	return COAP_OK;
}




CoAP_Result_t _rom CoAP_RemoveInteractionsObserver(CoAP_Interaction_t* pIA, uint64_t token) {

	return CoAP_RemoveObserverFromResource( &((pIA->pRes)->pListObservers), pIA->ifID , &(pIA->RemoteEp) , token);
}


//CoAP_Result_t CoAP_RemoveInteractionsObserver(CoAP_Interaction_t* pIA, uint64_t token) {
//	CoAP_Observer_t* pExistingObserver = (pIA->pRes)->pObserver;
//
//	while(pExistingObserver != NULL) { //found right existing observation -> delete it
//
//		if(token == pExistingObserver->Token && pIA->ifID == pExistingObserver->IfID && EpAreEqual(&(pIA->RemoteEp), &(pExistingObserver->Ep))) {
//			INFO("- (!) Unlinking observer from resource\r\n");
//			CoAP_UnlinkObserverFromList(&((pIA->pRes)->pObserver), pExistingObserver, true);
//			return COAP_REMOVED;
//		}
//		pExistingObserver = pExistingObserver->next;
//	}
//}



CoAP_Result_t _rom CoAP_HandleObservationInReq(CoAP_Interaction_t* pIA) {
	CoAP_Result_t res;
	uint32_t obsVal = 0;
	CoAP_Observer_t* pObserver = NULL;
	CoAP_Observer_t* pExistingObserver = (pIA->pRes)->pListObservers;

	if(pIA->pRes->Notifier == NULL) return COAP_ERR_NOT_FOUND; 			//resource does not support observe
	if(pIA == NULL || pIA->pReqMsg == NULL) return COAP_ERR_ARGUMENT; 	//safety checks

	if( (res=GetObserveOptionFromMsg(pIA->pReqMsg, &obsVal)) != COAP_OK) return res; //if no observe option in req function can't do anything

	//Client registers
	if(obsVal == OBSERVE_OPT_REGISTER) { //val = 0

		//Alloc memory for new Observer
		pObserver = CoAP_AllocNewObserver();
		if(pObserver == NULL) return COAP_ERR_OUT_OF_MEMORY;

		//Copy relevant information for observation from current interaction
		pObserver->Ep = pIA->RemoteEp;
		pObserver->IfID = pIA->ifID;
		pObserver->Token = pIA->pReqMsg->Token64;
		pObserver->next = NULL;

		//Copy relevant Options from Request (uri-query, observe)
		//Note: uri-path is not relevant since observers are fixed to its resource
		CoAP_option_t* pOption = pIA->pReqMsg->pOptionsList;
		while(pOption != NULL) {
			if(pOption->Number == OPT_NUM_URI_QUERY || pOption->Number == OPT_NUM_OBSERVE) {
				//create copy from volatile Iinteraction msg options
				if(CoAP_AppendOptionToList( &(pObserver->pOptList), pOption->Number, pOption->Value, pOption->Length)!=COAP_OK) {
					CoAP_FreeObserver(&pObserver);
					return COAP_ERR_OUT_OF_MEMORY;
				}
			}
			pOption = pOption->next;
		}

		//delete eventually existing same observer
		while(pExistingObserver != NULL) { //found right existing observation -> delete it
			if(pIA->pReqMsg->Token64 == pExistingObserver->Token && pIA->ifID == pExistingObserver->IfID && EpAreEqual(&(pIA->RemoteEp), &(pExistingObserver->Ep))) {
				CoAP_UnlinkObserverFromList(&((pIA->pRes)->pListObservers), pExistingObserver, true);
				break;
			}
			pExistingObserver = pExistingObserver->next;
		}

		//attach/update observer to resource
		return CoAP_AppendObserverToList( & ((pIA->pRes)->pListObservers), pObserver);

	//Client cancels observation actively (this is an alternative to simply forget the req token and send rst on next notification)
	} else if(obsVal == OBSERVE_OPT_DEREGISTER) {
		//find matching observer in resource observers-list
		CoAP_RemoveInteractionsObserver(pIA, pIA->pReqMsg->Token64); //remove observer identified by token, ifID and remote EP from resource

		//delete/abort any pending notification interaction
		CoAP_Interaction_t* pIApending;
		for(pIApending = CoAP.pInteractions; pIApending!=NULL; pIApending=pIApending->next) {
			if(pIApending->Role == COAP_ROLE_NOTIFICATION) {
				if(pIApending->pRespMsg->Token64 == pIA->pReqMsg->Token64 &&  pIApending->ifID == pIA->ifID && EpAreEqual(&(pIApending->RemoteEp), &(pIA->RemoteEp))) {
					INFO("Abort of pending notificaton interaction\r\n");
					CoAP_DeleteInteraction(pIApending);
					break;
				}
			}
		}

	} else return COAP_BAD_OPTION_VAL;

	return COAP_ERR_NOT_FOUND;
}
