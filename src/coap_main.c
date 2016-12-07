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
#include "liblobaro_coap.h"

CoAP_t CoAP = {.pInteractions = NULL, .api = {0}, .cfg = {0}};

void hal_debug_puts(char* s) {
	if (CoAP.api.debugPuts != NULL) {
		CoAP.api.debugPuts(s);
	}
}

void hal_debug_putc(char c) {
	if (CoAP.api.debugPutc != NULL) {
		CoAP.api.debugPutc(c);
	}
}

// Called by network interfaces to pass rawData which is parsed to CoAP messages.
// lifetime of pckt only during function invoke
// can be called from irq since more expensive work is done in CoAP_doWork loop
void _ram CoAP_HandleIncommingPacket(SocketHandle_t socketHandle, NetPacket_t* pPacket) {
	CoAP_Message_t* pMsg = NULL;
	bool isRequest = false;
	CoAP_Res_t* pRes = NULL;
	CoAP_Result_t res = COAP_OK;

	// Try to parse packet of bytes into CoAP message
	INFO("\r\no<<<<<<<<<<<<<<<<<<<<<<\r\nNew Datagram received [%d Bytes], Interface #%x\r\n", pPacket->size, socketHandle); //PrintRawPacket(pckt);
	INFO("Sending Endpoint: ");
	PrintEndpoint(&(pPacket->remoteEp));

	if ((res = CoAP_ParseMessageFromDatagram(pPacket->pData, pPacket->size, &pMsg)) == COAP_OK) {
		CoAP_PrintMsg(pMsg); // allocates the needed amount of ram
		INFO("o<<<<<<<<<<<<<<<<<<<<<<\r\n");
	} else {
		ERROR("ParseResult: ");
		CoAP_PrintResultValue(res);
		INFO("o<<<<<<<<<<<<<<<<<<<<<<\r\n");
		return; //very early parsing fail, coap parse was a total fail can't do anything for remote user, complete ignore of packet
	}

	isRequest = CoAP_MsgIsRequest(pMsg);

	// Filter out bad CODE/TYPE combinations (Table 1, RFC7252 4.3.) by silently ignoring them
	if (pMsg->Type == CON && pMsg->Code == EMPTY) {
		CoAP_SendEmptyRST(pMsg->MessageID, socketHandle, pPacket->remoteEp); //a.k.a "CoAP Ping"
		CoAP_free_Message(&pMsg); //free if not used inside interaction
		coap_mem_stats();
		return;
	} else if (pMsg->Type == ACK && isRequest) {
		goto END;
	} else if (pMsg->Type == RST && pMsg->Code != EMPTY) {
		goto END;
	} else if (pMsg->Type == NON && pMsg->Code == EMPTY) {
		goto END;
	}

	// Requested uri present?
	// Then call the handler, else send 4.04 response
	if (isRequest) {
		pRes = CoAP_FindResourceByUri(NULL, pMsg->pOptionsList);
		if (pRes == NULL || pRes->Handler == NULL) { //unknown resource requested
			if (pMsg->Type == CON) {
				CoAP_SendShortResp(ACK, RESP_NOT_FOUND_4_04, pMsg->MessageID, pMsg->Token64, socketHandle, pPacket->remoteEp);
			} else { // usually NON, but we better catch all
				CoAP_SendShortResp(NON, RESP_NOT_FOUND_4_04, CoAP_GetNextMid(), pMsg->Token64, socketHandle, pPacket->remoteEp);
			}
			goto END;
		}
	}

	// Unknown critical Option check
	uint16_t criticalOptNum = CoAP_CheckForUnknownCriticalOption(pMsg->pOptionsList); // !=0 if at least one unknown option found
	if (criticalOptNum) {
		INFO("- (!) Received msg has unknown critical option!!!\r\n");
		if (pMsg->Type == NON || pMsg->Type == ACK) {
			// NON messages are just silently ignored
			goto END;
		} else if (pMsg->Type == CON) {
			if (isRequest) {
				//todo: add diagnostic payload which option rejectet
				CoAP_SendShortResp(ACK, RESP_BAD_OPTION_4_02, pMsg->MessageID, pMsg->Token64, socketHandle, pPacket->remoteEp);
			} else {
				//reject externals servers response
				CoAP_SendEmptyRST(pMsg->MessageID, socketHandle, pPacket->remoteEp);
			}
		}
		goto END;
	}

	//*****************
	// Prechecks done
	//*****************

	// try to include message into new or existing server/client interaction
	switch (pMsg->Type) {
		case RST: {
			if (CoAP_ApplyReliabilityStateToInteraction(RST_SET, pMsg->MessageID, &(pPacket->remoteEp)) == NULL) {
				INFO("- (?) Got Reset on (no more?) existing message id: %d\r\n", pMsg->MessageID);
			}
			goto END;
		}
		case ACK: {
			CoAP_Interaction_t* pIA = NULL;
			// apply "ACK received" to req (client) or resp (server)
			if ((pIA = CoAP_ApplyReliabilityStateToInteraction(ACK_SET, pMsg->MessageID, &(pPacket->remoteEp))) == NULL) {
				INFO("- (?) Got ACK on (no more?) existing message id: %d\r\n", pMsg->MessageID);
				goto END;
			}
			//piA is NOT NULL in every case here
			INFO("- piggybacked response received\r\n");
			if (pMsg->Code != EMPTY) {
				//no "simple" ACK => must be piggybacked RESPONSE to our [client] request. corresponding Interaction has been found before
				if (pIA->Role == COAP_ROLE_CLIENT && pIA->pReqMsg->Token64 == pMsg->Token64 && pIA->State == COAP_STATE_WAITING_RESPONSE) {
					if (pIA->pRespMsg != NULL) {
						CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
					}
					pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
					pIA->State = COAP_STATE_HANDLE_RESPONSE;
					return;
				} else {
					INFO("- could not piggybacked response to any request!\r\n");
				}
			}
			break;
		}
		case NON:
		case CON: {
			if (isRequest) {
				// we act as a CoAP Server
				res = CoAP_StartNewServerInteraction(pMsg, pRes, socketHandle, pPacket);

				if (res == COAP_OK) {
					// new interaction process started (handled by CoAP_doWork())
					return;
				} else if (res == COAP_ERR_EXISTING) {
					//duplicated request detected by messageID
					goto END;
				} else if (res == COAP_ERR_OUT_OF_MEMORY) {
					if (pMsg->Type == CON) {
						// will free any already allocated mem
						CoAP_SendShortResp(ACK, RESP_INTERNAL_SERVER_ERROR_5_00, pMsg->MessageID, pMsg->Token64, socketHandle, pPacket->remoteEp);
					}
					goto END;
				}

			} else { // pMsg carries a separate response (=no piggyback!) to our client request...
				// find in interaction list request with same token & endpoint
				CoAP_Interaction_t* pIA;
				for (pIA = CoAP.pInteractions; pIA != NULL; pIA = pIA->next) {
					if (pIA->Role == COAP_ROLE_CLIENT && pIA->pReqMsg->Token64 == pMsg->Token64 && EpAreEqual(&(pPacket->remoteEp), &(pIA->RemoteEp))) {
						// 2nd case "updates" received response
						if (pIA->State == COAP_STATE_WAITING_RESPONSE || pIA->State == COAP_STATE_HANDLE_RESPONSE) {
							if (pIA->pRespMsg != NULL) {
								CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
							}
							pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
							pIA->State = COAP_STATE_HANDLE_RESPONSE;
						}

						if (pMsg->Type == CON) {
							if (CoAP_SendShortResp(ACK, EMPTY, pMsg->MessageID, pMsg->Token64, socketHandle, pPacket->remoteEp) == COAP_OK) {
								pIA->RespReliabilityState = ACK_SET;
							}
						}
						return;
					}
				} // for loop

				// no active interaction found to match remote msg to...
				// no matching IA has been found! can't do anything with this msg -> Rejecting it (also NON msg) (see RFC7252, 4.3.)
				CoAP_SendShortResp(RST, EMPTY, pMsg->MessageID, pMsg->Token64, socketHandle, pPacket->remoteEp);
				goto END;
			}

			break;
		}
		default: {
			goto END;
		}
	}
	END: // only reached if no interaction has been started (return statement)
	CoAP_free_Message(&pMsg); // free if not used inside interaction
}

static CoAP_Result_t _rom SendResp(CoAP_Interaction_t* pIA, CoAP_InteractionState_t nextIAState) {
	if (CoAP_SendMsg(pIA->pRespMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {

		if (pIA->pRespMsg->Type == ACK) { //piggy back resp
			pIA->ReqReliabilityState = ACK_SET;
		} else if (pIA->pRespMsg->Type == CON) {
			CoAP_EnableAckTimeout(pIA, pIA->RetransCounter); //enable timeout on waiting for ack
		}//else NON (no special handling=

		pIA->State = nextIAState; //move to next state

		CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing//todo: in die äußere statemachine

	} else { //unexspected internal failure todo: try at least to send 4 byte RESP_INTERNAL_SERVER_ERROR_5_00
		INFO("(!!!) SendResp(): Internal socket error on sending response! MiD: %d", pIA->pReqMsg->MessageID);
		CoAP_DeleteInteraction(pIA);
		return COAP_ERR_SOCKET;
	}

	return COAP_OK;
}

static CoAP_Result_t _rom SendReq(CoAP_Interaction_t* pIA, CoAP_InteractionState_t nextIAState) {
	if (CoAP_SendMsg(pIA->pReqMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {

		if (pIA->pReqMsg->Type == CON) {
			CoAP_EnableAckTimeout(pIA, pIA->RetransCounter); //enable timeout on waiting for ack
		}//else NON (no special handling=

		pIA->State = nextIAState; //move to next state

		CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing//todo: in die äußere statemachine

	} else { //unexspected internal failure todo: try at least to send 4 byte RESP_INTERNAL_SERVER_ERROR_5_00
		INFO("(!!!) SendReq(): Internal socket error on sending response! MiD: %d", pIA->pReqMsg->MessageID);
		CoAP_DeleteInteraction(pIA);
		return COAP_ERR_SOCKET;
	}

	return COAP_OK;
}

//used on [server]
static CoAP_Result_t _rom CheckRespStatus(CoAP_Interaction_t* pIA) {

	if (pIA->RespReliabilityState == RST_SET) {
		INFO("- Response reset by remote client -> Interaction aborted\r\n");
		//todo: call failure callback to user
		return COAP_ERR_REMOTE_RST;
	}

	if (pIA->pRespMsg->Type == NON || pIA->pRespMsg->Type == ACK) {
		//if(CoAP_MsgIsOlderThan(pIA->pRespMsg, HOLDTIME_AFTER_TRANSACTION_END)) CoAP_DeleteInteraction(pIA); //hold if new request with same token occurs, e.g. response was lost
		//else CoAP_SetSleepInteraction(pIA, 2); //check back in 2 sec. todo: would it be better to sleep for total holdtime?
		if (CoAP_MsgIsOlderThan(pIA->pRespMsg, HOLDTIME_AFTER_NON_TRANSACTION_END)) {
			return COAP_OK;
		}
		return COAP_HOLDING_BACK;

	} else if (pIA->pRespMsg->Type == CON) {
		if (pIA->RespReliabilityState == ACK_SET) { //everything fine!
			INFO("- Response ACKed by Client -> Transaction ended succesfully\r\n");
			//todo: call success callback to user
			return COAP_OK;
		} else { //check ACK/RST timeout of our CON response
			if (CoAP.api.rtc1HzCnt() > pIA->AckTimeout) {
				if (pIA->RetransCounter + 1 > MAX_RETRANSMIT) { //give up
					INFO("- (!) ACK timeout on sending response, giving up! Resp.MiD: %d\r\n",
						 pIA->pRespMsg->MessageID);
					return COAP_ERR_OUT_OF_ATTEMPTS;
				} else {
					INFO("- (!) Retry num %d\r\n", pIA->RetransCounter + 1);
					return COAP_RETRY;
				}
			} else {
				return COAP_WAITING;
			}
		}
	}

	INFO("(!!!) CheckRespStatus(...) COAP_ERR_ARGUMENT !?!?\r\n");
	return COAP_ERR_ARGUMENT;
}

//used on [CLIENT] side request to check progress of interaction
static CoAP_Result_t _rom CheckReqStatus(CoAP_Interaction_t* pIA) {

	if (pIA->ReqReliabilityState == RST_SET) {
		INFO("- Response reset by remote server -> Interaction aborted\r\n");
		//todo: call failure callback to user
		return COAP_ERR_REMOTE_RST;
	}

	if (pIA->pReqMsg->Type == CON) {

		if (pIA->ReqReliabilityState == ACK_SET) {
			if (CoAP_MsgIsOlderThan(pIA->pReqMsg, CLIENT_MAX_RESP_WAIT_TIME)) {
				INFO("- Request ACKed separate by server, but giving up to wait for acutal response data\r\n");
				return COAP_ERR_TIMEOUT;
			}

			INFO("- Request ACKed separate by server -> Waiting for actual response\r\n");
			return COAP_WAITING;
		} else { //check ACK/RST timeout of our CON request
			if (CoAP.api.rtc1HzCnt() > pIA->AckTimeout) {
				if (pIA->RetransCounter + 1 > MAX_RETRANSMIT) { //give up
					INFO("- (!) ACK timeout on sending request, giving up! MiD: %d", pIA->pReqMsg->MessageID);
					return COAP_ERR_OUT_OF_ATTEMPTS;
				} else {
					INFO("- (!) Retry num %d\r\n", pIA->RetransCounter + 1);
					return COAP_RETRY;
				}
			} else {
				return COAP_WAITING;
			}
		}
	} else { // request type = NON
		if (CoAP_MsgIsOlderThan(pIA->pReqMsg, CLIENT_MAX_RESP_WAIT_TIME)) {
			INFO("- [NON request]: Giving up to wait for actual response data\r\n");
			return COAP_ERR_TIMEOUT;
		} else return COAP_WAITING;
	}

	INFO("(!!!) CheckReqStatus(...) COAP_ERR_ARGUMENT !?!?\r\n");
	return COAP_ERR_ARGUMENT;
}


CoAP_Socket_t* CoAP_NewSocket(SocketHandle_t handle) {
	CoAP_Socket_t* socket = AllocSocket();
	socket->Handle = handle;
	return socket;
}

//must be called regularly
void _rom CoAP_doWork() {
	CoAP_Interaction_t* pIA = CoAP_GetLongestPendingInteraction();
	CoAP_Result_t result;


	if (pIA == NULL) {
		//nothing to do now
		return;
	}
	if (pIA->SleepUntil > CoAP.api.rtc1HzCnt()) {
		CoAP_EnqueueLastInteraction(pIA);
		return;
	}

	//	INFO("pending Transaction found! ReqTime: %u\r\n", pIA->ReqTime);
	//	com_mem_stats();

//########################
//###   ROLE Server    ###
//########################
	if(pIA->Role == COAP_ROLE_SERVER)
	{
	  //--------------------------------------------------------------------------------------------------------
		if(pIA->State == COAP_STATE_HANDLE_REQUEST ||
		   pIA->State == COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT ||
		   pIA->State == COAP_STATE_RESPONSE_WAITING_LEISURE)
	  //--------------------------------------------------------------------------------------------------------
		{
			if(pIA->ReqMetaInfo.Type == META_INFO_MULTICAST){
				// Messages sent via multicast MUST be NON-confirmable.
				if(pIA->pReqMsg->Type == CON){
					INFO("Request received from multicast endpoint is not allowed");
					CoAP_DeleteInteraction(pIA);
					return;
				}
				// Multicast messages get a response after a leisure period.
				if(pIA->State == COAP_STATE_HANDLE_REQUEST){
					pIA->State = COAP_STATE_RESPONSE_WAITING_LEISURE;

					// Todo: Pick a random leisure period (See section 8.2 of [RFC7252])
					CoAP_SetSleepInteraction(pIA, DEFAULT_LEISURE); // Don't respond right away'
					CoAP_EnqueueLastInteraction(pIA);
					INFO("Multicast request postponed processing until %d\r\n", pIA->SleepUntil);
					return;
				}
			}

			if( 	((pIA->pReqMsg->Code == REQ_GET) && !((pIA->pRes->Options).Flags & RES_OPT_GET))
				||  ((pIA->pReqMsg->Code == REQ_POST) && !((pIA->pRes->Options).Flags & RES_OPT_POST))
				||  ((pIA->pReqMsg->Code == REQ_PUT) && !((pIA->pRes->Options).Flags & RES_OPT_PUT))
				||  ((pIA->pReqMsg->Code == REQ_DELETE) && !((pIA->pRes->Options).Flags & RES_OPT_DELETE))
			){
				pIA->pRespMsg = CoAP_AllocRespMsg(pIA->pReqMsg, RESP_METHOD_NOT_ALLOWED_4_05, 0); //matches also TYPE + TOKEN to request

				//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
				//transmit response & move to next state
				SendResp(pIA, COAP_STATE_RESPONSE_SENT);
				//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
				return;
			}
			// (else) request method supported by resource...:

			// Allocate new msg with payload buffer which can be directly used OR overwritten
			// by resource handler with (ownstatic memory OR  "com_mem_get(...)" memory areas.
			// Any non static memory will be freed along with message! see free_Payload(...) function, even if user overwrites payload pointer!
			if (pIA->pRespMsg == NULL) { //if postponed before it would have been already allocated
				pIA->pRespMsg = CoAP_AllocRespMsg(pIA->pReqMsg, EMPTY,
												  PREFERED_PAYLOAD_SIZE); //matches also TYPE + TOKEN to request
			}

			// Call of external set resource handler
			// could change type and code of message (ACK & EMPTY above only a guess!)
			CoAP_HandlerResult_t Res = pIA->pRes->Handler(pIA->pReqMsg, pIA->pRespMsg);

			// Check return value of handler:
			// a) everything fine - we got an response to send
			if (Res == HANDLER_OK && pIA->pRespMsg->Code == EMPTY) {
				pIA->pRespMsg->Code = RESP_SUCCESS_CONTENT_2_05; //handler forgot to set code?

				// b) handler has no result and will not deliver	in the future
			} else if (Res == HANDLER_ERROR && pIA->pRespMsg->Code == EMPTY) {
				pIA->pRespMsg->Code = RESP_INTERNAL_SERVER_ERROR_5_00; //handler forgot to set code?

				// Don't respond with reset or empty messages to requests originating from multicast enpoints
				if(pIA->ReqMetaInfo.Type == META_INFO_MULTICAST) {
					CoAP_DeleteInteraction(pIA);
					return;
				}

				// c) handler needs some more time
			} else if (Res == HANDLER_POSTPONE) { // Handler needs more time to fulfill request, send ACK and separate response
				if (pIA->pReqMsg->Type == CON && pIA->ReqReliabilityState != ACK_SET) {
					if (CoAP_SendEmptyAck(pIA->pReqMsg->MessageID, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {

						pIA->ReqReliabilityState = ACK_SET;
						pIA->State = COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT;
						// give resource some time to become ready
						pIA->SleepUntil = CoAP.api.rtc1HzCnt() + POSTPONE_WAIT_TIME_SEK;

						CoAP_EnqueueLastInteraction(pIA);
						INFO("Resource not ready, postponed response until %d\r\n", pIA->SleepUntil);
						return;
					} else { // unexspected internal failure todo: try at least to send 4 byte RESP_INTERNAL_SERVER_ERROR_5_00
						INFO("(!!!) Send Error on empty ack, MiD: %d", pIA->pReqMsg->MessageID);
						CoAP_DeleteInteraction(pIA);
						return;
					}
				}

				//Timeout on postpone?
				if (CoAP_MsgIsOlderThan(pIA->pReqMsg, POSTPONE_MAX_WAIT_TIME)) {
					pIA->pRespMsg->Code = RESP_SERVICE_UNAVAILABLE_5_03;
				} else {
					CoAP_SetSleepInteraction(pIA, POSTPONE_WAIT_TIME_SEK);
					CoAP_EnqueueLastInteraction(pIA); //give resource some time to become ready
					return;
				}
			}

			//Set response TYPE correctly if CON request, regardless of what the handler did to this resp msg field, it can't know it better :-)
			//on NON requests the handler can decide if use CON or NON in response (default is also using NON in response)
			if (pIA->pReqMsg->Type == CON) {
				if (pIA->ReqReliabilityState ==
					ACK_SET) { //separate empty ACK has been sent before (piggyback-ack no more possible)
					pIA->pRespMsg->Type = CON;
					pIA->pRespMsg->MessageID = CoAP_GetNextMid(); //we must use/generate a new messageID;
				} else pIA->pRespMsg->Type = ACK; //"piggybacked ack"
			}

			//Add custom option #10000 with hopcount and rssi of request to response
			if (pIA->ReqMetaInfo.Type == META_INFO_RF_PATH) {
				uint8_t buf_temp[2];
				buf_temp[0] = pIA->ReqMetaInfo.Dat.RfPath.HopCount;
				buf_temp[1] = pIA->ReqMetaInfo.Dat.RfPath.RSSI * -1;
				CoAP_AppendOptionToList(&(pIA->pRespMsg->pOptionsList), 10000, buf_temp, 2); //custom option #10000
			}

			//handle for GET observe option
			if (pIA->pReqMsg->Code == REQ_GET && pIA->pRespMsg->Code == RESP_SUCCESS_CONTENT_2_05) {

				if ((result = CoAP_HandleObservationInReq(pIA)) == COAP_OK) { //<---- attach OBSERVER to resource
					AddObserveOptionToMsg(pIA->pRespMsg, 0);  //= ACK observation to client
					INFO("- Observation activated\r\n");
				} else if (result == COAP_REMOVED) {
					INFO("- Observation actively removed by client\r\n");
				}

			}

			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			SendResp(pIA, COAP_STATE_RESPONSE_SENT); //transmit response & move to next state
			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			return;
			//--------------------------------------------------
		} else if (pIA->State == COAP_STATE_RESPONSE_SENT) {
			//--------------------------------------------------
			switch (CheckRespStatus(pIA)) {
				case COAP_WAITING:
				case COAP_HOLDING_BACK:
					CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing
					break;

				case COAP_RETRY:
					if (CoAP_SendMsg(pIA->pRespMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {
						pIA->RetransCounter++;
						CoAP_EnableAckTimeout(pIA, pIA->RetransCounter);
					} else {
						INFO("(!!!) Internal socket error on sending response! MiD: %d", pIA->pReqMsg->MessageID);
						CoAP_DeleteInteraction(pIA);
					}
					break;

				case COAP_ERR_OUT_OF_ATTEMPTS:
				case COAP_ERR_REMOTE_RST:
				default:
					CoAP_DeleteInteraction(pIA);
			}
		}
		return;
		//########################
		//### ROLE NOTIFICATOR ###
		//########################
	} else if (pIA->Role == COAP_ROLE_NOTIFICATION) {

		//------------------------------------------
		if (pIA->State == COAP_STATE_READY_TO_NOTIFY) {
			//------------------------------------------



			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			SendResp(pIA, COAP_STATE_NOTIFICATION_SENT); //transmit response & move to next state
			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			//--------------------------------------------------
		} else if (pIA->State == COAP_STATE_NOTIFICATION_SENT) {
			//--------------------------------------------------
			switch (CheckRespStatus(pIA)) {
				case COAP_WAITING:
				case COAP_HOLDING_BACK:
					CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing
					break;

				case COAP_RETRY:

#if USE_RFC7641_ADVANCED_TRANSMISSION == 1
					//Implement RFC7641 (observe) "4.5.2.  Advanced Transmission"
					//Effectively abort previous notification and send a fresher one
					//retain transmission parameters of "pending" interaction
					if (pIA->UpdatePendingNotification) {
						CoAP_MessageType_t TypeSave = pIA->pRespMsg->Type;
						INFO("in retry: update pending IA\r\n");
						pIA->UpdatePendingNotification = false;
						pIA->pRespMsg->MessageID = CoAP_GetNextMid();
						//call notifier
						if (pIA->pRes->Notifier(pIA->pObserver, pIA->pRespMsg) >= RESP_ERROR_BAD_REQUEST_4_00) {
							RemoveObserveOptionFromMsg(pIA->pRespMsg);
							CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token64);
						} else { //good response
							UpdateObserveOptionInMsg(pIA->pRespMsg, pIA->pRes->UpdateCnt);
						}
						pIA->pRespMsg->Type = TypeSave; //Type of resent should stay the same, e.g. one CON between many NON messages should be preserved
					}
#endif

					if (CoAP_SendMsg(pIA->pRespMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {
						pIA->RetransCounter++;
						CoAP_EnableAckTimeout(pIA, pIA->RetransCounter);
						INFO("- Changed notification body during retry\r\n");
					} else {
						INFO("(!!!) Internal socket error on sending response! MiD: %d\r\n", pIA->pReqMsg->MessageID);
						CoAP_DeleteInteraction(pIA);
					}
					break;

				case COAP_OK:

#if USE_RFC7641_ADVANCED_TRANSMISSION == 1
					if (pIA->UpdatePendingNotification) {
						//Implement RFC7641 (observe) "4.5.2.  Advanced Transmission" and send a fresher representation
						//also reset transmission parameters (since previous transfer ended successfully)
						pIA->State = COAP_STATE_READY_TO_NOTIFY;
						pIA->RetransCounter = 0;
						pIA->UpdatePendingNotification = false;
						pIA->pRespMsg->MessageID = CoAP_GetNextMid();
						pIA->RespReliabilityState = NOT_SET;

						//call notifier
						if (pIA->pRes->Notifier(pIA->pObserver, pIA->pRespMsg) >= RESP_ERROR_BAD_REQUEST_4_00) {
							RemoveObserveOptionFromMsg(pIA->pRespMsg);
							CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token64);
						} else { //good response
							UpdateObserveOptionInMsg(pIA->pRespMsg, pIA->pRes->UpdateCnt);
						}
						INFO("- Started new notification since resource has been updated!\r\n");
						CoAP_EnqueueLastInteraction(pIA);
						return;
					}
#endif
					CoAP_DeleteInteraction(pIA); //done!
					break;

				case COAP_ERR_OUT_OF_ATTEMPTS: //check is resource is a lazy observe delete one
				case COAP_ERR_REMOTE_RST:
					CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token64);  //remove observer from resource
				default:
					CoAP_DeleteInteraction(pIA);
					break;
			}
		}
		return;

		//########################
		//###    ROLE Client   ###
		//########################
	} else if (pIA->Role == COAP_ROLE_CLIENT) {

		//------------------------------------------
		if (pIA->State == COAP_STATE_READY_TO_REQUEST) {
			//------------------------------------------
			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			SendReq(pIA, COAP_STATE_WAITING_RESPONSE); //transmit response & move to next state
			//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			//--------------------------------------------------
		} else if (pIA->State == COAP_STATE_WAITING_RESPONSE) {
			//--------------------------------------------------
			switch (CheckReqStatus(pIA)) {
				case COAP_WAITING:
					CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing
					break;

				case COAP_RETRY:
					if (CoAP_SendMsg(pIA->pReqMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {
						pIA->RetransCounter++;
						CoAP_EnableAckTimeout(pIA, pIA->RetransCounter);
					} else {
						INFO("(!!!) Internal socket error on sending request retry! MiD: %d\r\n",
							 pIA->pReqMsg->MessageID);
						CoAP_DeleteInteraction(pIA);
					}
					break;

				case COAP_ERR_OUT_OF_ATTEMPTS: //check is resource is a lazy observe delete one
				case COAP_ERR_REMOTE_RST:
				case COAP_ERR_TIMEOUT:
				default:
					CoAP_DeleteInteraction(pIA);
			}
			//--------------------------------------------------
		} else if (pIA->State == COAP_STATE_HANDLE_RESPONSE) {
			//--------------------------------------------------
			INFO("- Got Response to Client request! -> calling Handler!\r\n");
			if (pIA->RespCB != NULL) {
				pIA->RespCB(pIA->pRespMsg, &(pIA->RemoteEp)); //call callback
			}

//			pIA->State = COAP_STATE_FINISHED;
//			CoAP_EnqueueLastInteraction(pIA);
			CoAP_DeleteInteraction(
					pIA); //direct delete, todo: eventually wait some time to send ACK instead of RST if out ACK to remote reponse was lost

		} else {
			CoAP_DeleteInteraction(pIA); //unknown state, should not go here
		}
	}

	return;
}



