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
#include <inttypes.h>
#include "coap.h"
#include "liblobaro_coap.h"

CoAP_t CoAP = { .pInteractions = NULL, .api = { 0 } };

void hal_debug_puts(char* s) {
	if (CoAP.api.debugPuts != NULL) {
		CoAP.api.debugPuts(s);
	}
}

void hal_debug_array(const char *s, const uint8_t *array, size_t size) {
	if (CoAP.api.debugArray != NULL) {
		CoAP.api.debugArray(s, array, size);
	}
}

void _ram CoAP_HandleLowerLayerReceiverError(SocketHandle_t socketHandle, NetPacket_t* pPacket, 
                                             CoAP_MessageCode_t responseCode) {
	CoAP_Message_t Msg;
	uint16_t optionsOfsset=0;
	if(COAP_OK != CoAP_ParseDatagramUpToToken(pPacket->pData, pPacket->size, &Msg, &optionsOfsset))
	{
		ERROR("CoAP_HandleLowerLayerError failed. Dropping packet.");
		return;
	}
	CoAP_SendResponseWithoutPayload(responseCode, &Msg, socketHandle, pPacket->remoteEp, NULL);
}

void _ram CoAP_HandleResponseWithEcho(SocketHandle_t socketHandle, NetPacket_t* pPacket, 
                                      uint8_t *echoValue, size_t echoValueLength) {
	CoAP_Message_t msg;
	uint16_t optionsOfsset=0;
	if(COAP_OK != CoAP_ParseDatagramUpToToken(pPacket->pData, pPacket->size, &msg, &optionsOfsset))
	{
		ERROR("CoAP_HandleResponseWithEcho failed. Dropping packet.");
		return;
	}

	CoAP_PrepareResponseWithEcho(&msg, echoValue, echoValueLength);
	CoAP_SendMsg(&msg, socketHandle, pPacket->remoteEp);
	CoAP_FreeOptionList(&(msg.pOptionsList));
}


// Called by network interfaces to pass rawData which is parsed to CoAP messages.
// lifetime of pckt only during function invoke
// can be called from irq since more expensive work is done in CoAP_doWork loop
void _ram CoAP_HandleIncomingPacket(SocketHandle_t socketHandle, NetPacket_t* pPacket) {
	CoAP_Message_t* pMsg = NULL;
	bool isRequest = false;
	CoAP_Res_t* pRes = NULL;
	CoAP_Result_t res = COAP_OK;

	// Try to parse packet of bytes into CoAP message
	INFO("\r\no<<<<<<<<<<<<<<<<<<<<<<\r\nNew Datagram received [%d Bytes], Interface #%p\r\n", pPacket->size, socketHandle); //PrintRawPacket(pckt);
	INFO("Sending Endpoint: ");
	PrintEndpoint(&(pPacket->remoteEp));
	INFO("\n\r");

	if ((res = CoAP_ParseMessageFromDatagram(pPacket->pData, pPacket->size, &pMsg)) == COAP_OK) {
		CoAP_PrintMsg(pMsg); // allocates the needed amount of ram
		INFO("o<<<<<<<<<<<<<<<<<<<<<<\r\n");
	} else if(res == COAP_PARSE_TOO_MUCH_PAYLOAD){
		// RFC 7252, section 5.9.2.9.
		CoAP_option_t *pOptionsList = NULL;
		if(COAP_OK != CoAP_AppendUintOptionToList(&pOptionsList, OPT_NUM_SIZE1, MAX_PAYLOAD_SIZE))
		{
			ERROR("Failed to create option SIZE1\r\n");
			//not a critical error, continue
		}
		if(COAP_OK != CoAP_SendResponseWithoutPayload(RESP_REQUEST_ENTITY_TOO_LARGE_4_13, pMsg, socketHandle, pPacket->remoteEp, pOptionsList))
		{
			ERROR("Failed to send response 4.13.\r\n");
			//can't do anything to repair/cleanup the problem - continue.
		}
		CoAP_FreeOptionList(&pOptionsList);
		goto END;
	} else {
		ERROR("ParseResult: ");
		CoAP_PrintResultValue(res);
		INFO("o<<<<<<<<<<<<<<<<<<<<<<\r\n");
		return; //very early parsing fail, coap parse was a total fail can't do anything for remote user, complete ignore of packet
	}

#if DEBUG_RANDOM_DROP_INCOMING_PERCENTAGE > 0
	if (CoAP.api.rand() % 100 < DEBUG_RANDOM_DROP_INCOMING_PERCENTAGE) {
		INFO("!!!FAIL!!! on purpose, dropping incoming message (%d%% chance)\n", DEBUG_RANDOM_DROP_INCOMING_PERCENTAGE);
		CoAP_free_Message(&pMsg);
		return;
	}
#endif

	isRequest = CoAP_MsgIsRequest(pMsg);

	DEBUG("Filter out bad CODE/TYPE combinations\r\n");
	// Filter out bad CODE/TYPE combinations (Table 1, RFC7252 4.3.) by silently ignoring them
	if (pMsg->Type == CON && pMsg->Code == EMPTY) {
		CoAP_SendEmptyRST(pMsg->MessageID, socketHandle, pPacket->remoteEp); //a.k.a "CoAP Ping"
		CoAP_free_Message(&pMsg); //free if not used inside interaction
		// coap_mem_stats();
		return;
	} else if (pMsg->Type == ACK && isRequest) {
		goto END;
	} else if (pMsg->Type == RST && pMsg->Code != EMPTY) {
		goto END;
	} else if (pMsg->Type == NON && pMsg->Code == EMPTY) {
		goto END;
	}

	//INFO("Find the request handler or send 4.04\r\n");
	// Requested uri present?
	// Then find the handler, else send 4.04 response
	if (isRequest) {
		pRes = CoAP_FindResourceByUri(NULL, pMsg->pOptionsList);
		if (pRes == NULL || pRes->Handler == NULL) { //unknown resource requested
			if (pMsg->Type == CON) {
				CoAP_SendShortResp(ACK, RESP_NOT_FOUND_4_04, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp);
			} else { // usually NON, but we better catch all
				CoAP_SendShortResp(NON, RESP_NOT_FOUND_4_04, CoAP_GetNextMid(), pMsg->Token, socketHandle, pPacket->remoteEp);
			}
			goto END;
		}
	}

	//INFO("Check for critical options\r\n");
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
				CoAP_SendShortResp(ACK, RESP_BAD_OPTION_4_02, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp);
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

	//INFO("Prechecks done. Handle message by type\r\n");
	// try to include message into new or existing server/client interaction

	CoAP_Interaction_t* pIA = CoAP_FindInteractionByMessageIdAndEp(CoAP.pInteractions, pMsg->MessageID, &(pPacket->remoteEp));

	if (pIA != NULL) {
		pIA->SleepUntil = 0; // Wakeup interaction
	}

	switch (pMsg->Type) {
	case RST: {
		if (pIA == NULL) {
			INFO("- (?) Got Reset on (no more?) existing message id: %d\r\n", pMsg->MessageID);
			goto END;
		}
		pIA->ResConfirmState = RST_SEND;
		goto END;
	}
	case ACK: {
		// apply "ACK received" to req (client) or resp (server)
		if (pIA == NULL) {
			INFO("- (?) Got ACK on (no more?) existing message id: %d\r\n", pMsg->MessageID);
			goto END;
		}
		pIA->ResConfirmState = ACK_SEND;

		// Check Observation Option
		if (pIA->pReqMsg) {
			CoAP_option_t* observeOption = CoAP_FindOptionByNumber(pIA->pReqMsg, OPT_NUM_OBSERVE);
			if (pIA->Role == COAP_ROLE_CLIENT && observeOption != NULL && observeOption->Length > 0 && observeOption->Value[0] == 0x01) {
				pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
				pIA->State = COAP_STATE_HANDLE_RESPONSE;

				INFO("- Searching for original observe subscription interaction");
				for (CoAP_Interaction_t* pOIA = CoAP.pInteractions; pOIA != NULL; pOIA = pOIA->next) {
					INFO("compare token: %d message id: %d\r\n", CoAP_TokenEqual(pOIA->pReqMsg->Token, pIA->pReqMsg->Token), pOIA->pReqMsg->MessageID);
					if (pOIA->Role == COAP_ROLE_OBSERVATION && CoAP_TokenEqual(pOIA->pReqMsg->Token, pIA->pReqMsg->Token) && EpAreEqual(&(pIA->RemoteEp), &(pOIA->RemoteEp))) {
						INFO("- Found original observe subscription interaction message id: %d\r\n", pOIA->pReqMsg->MessageID);
						pOIA->State = COAP_STATE_FINISHED;
					}
				}
				return;
			}
		}
		//piA is NOT NULL in every case here
		DEBUG("- piggybacked response received\r\n");
		if (pMsg->Code != EMPTY) {
			//no "simple" ACK => must be piggybacked RESPONSE to our [client] request. corresponding Interaction has been found before
			if (pIA->Role == COAP_ROLE_CLIENT && CoAP_TokenEqual(pIA->pReqMsg->Token, pMsg->Token) && pIA->State == COAP_STATE_WAITING_RESPONSE) {
				if (pIA->pRespMsg != NULL) {
					CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
				}
				pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
				pIA->State = COAP_STATE_HANDLE_RESPONSE;
				return;
            }
            else if (pIA->Role == COAP_ROLE_OBSERVATION && CoAP_TokenEqual(pIA->pReqMsg->Token, pMsg->Token) && pIA->State == COAP_STATE_WAITING_RESPONSE) {
                if (pIA->pRespMsg != NULL) {
                    CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
                }
                pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
                pIA->State = COAP_STATE_HANDLE_NOTIFICATION;
                return;
            }
            else if(pIA->Role == COAP_ROLE_NOTIFICATION && CoAP_TokenEqual(pIA->pRespMsg->Token, pMsg->Token) && pIA->State == COAP_STATE_NOTIFICATION_SENT){
				INFO("- received acknowledgement on notification\r\n");
				return;
			}
			else {
				INFO("- could not piggybacked response to any request!\r\n");
			}
		}
        else {
            if (pIA->pReqMsg && pIA->pReqMsg->MessageID == pMsg->MessageID && pIA->State == COAP_STATE_WAITING_RESPONSE) {
                if (pIA->Role == COAP_ROLE_CLIENT) {
                    pIA->ReqConfirmState = ACK_SEND;
                }
                else if (pIA->Role == COAP_ROLE_OBSERVATION) {
                    pIA->State = COAP_STATE_WAITING_NOTIFICATION;
                }
                return;
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
					CoAP_SendShortResp(ACK, RESP_INTERNAL_SERVER_ERROR_5_00, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp);
				}
				goto END;
			}

		} else { // pMsg carries a separate response (=no piggyback!) to our client request...
			// find in interaction list request with same token & endpoint
			for (pIA = CoAP.pInteractions; pIA != NULL; pIA = pIA->next) {
                DEBUG("LOOPING UP pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pReqMsg->Code, pIA->pReqMsg->Type, pIA->Role);
                if (pIA->Role == COAP_ROLE_CLIENT && CoAP_TokenEqual(pIA->pReqMsg->Token, pMsg->Token) && EpAreEqual(&(pPacket->remoteEp), &(pIA->RemoteEp))) {
                    DEBUG("PICKING UP pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pReqMsg->Code, pIA->pReqMsg->Type, pIA->Role);
					// 2nd case "updates" received response
					if (pIA->State == COAP_STATE_WAITING_RESPONSE || pIA->State == COAP_STATE_HANDLE_RESPONSE) {
						if (pIA->pRespMsg != NULL) {
							CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
						}
						pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
                        pIA->State = COAP_STATE_HANDLE_RESPONSE;
					}

					if (pMsg->Type == CON) {
						if (CoAP_SendShortResp(ACK, EMPTY, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp) == COAP_OK) {
							pIA->ResConfirmState = ACK_SEND;
						}
					}
                    return;
				}
                
                if (pIA->Role == COAP_ROLE_OBSERVATION && CoAP_TokenEqual(pIA->pReqMsg->Token, pMsg->Token) && EpAreEqual(&(pPacket->remoteEp), &(pIA->RemoteEp))) {
                    DEBUG("PICKING UP pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pReqMsg->Code, pIA->pReqMsg->Type, pIA->Role);
                    // 2nd case "updates" received response
                    if (pIA->State == COAP_STATE_WAITING_RESPONSE ||pIA->State == COAP_STATE_WAITING_NOTIFICATION) {
                        if (pIA->pRespMsg != NULL) {
                            CoAP_free_Message(&(pIA->pRespMsg)); //free eventually present older response (todo: check if this is possible!?)
                        }
                        pIA->pRespMsg = pMsg; //attach just received message for further actions in IA [client] state-machine & return
                        pIA->State = COAP_STATE_HANDLE_NOTIFICATION;
                    }

                    if (pMsg->Type == CON) {
                        if (CoAP_SendShortResp(ACK, EMPTY, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp) == COAP_OK) {
                            pIA->ResConfirmState = ACK_SEND;
                        }
                    }
                    return;
                }

			} // for loop

			// no active interaction found to match remote msg to...
			// no matching IA has been found! can't do anything with this msg -> Rejecting it (also NON msg) (see RFC7252, 4.3.)
			CoAP_SendShortResp(RST, EMPTY, pMsg->MessageID, pMsg->Token, socketHandle, pPacket->remoteEp);
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

CoAP_Result_t _rom CoAP_removeObserver(uint32_t transport_ctx) {
    CoAP_Observer_t *pObserver = NULL;
    CoAP_Res_t *pRes = NULL;
    CoAP_Result_t res = CoAP_FindObserverAndResourceByTransportCtx(transport_ctx,
                                                                   &pObserver,
                                                                   &pRes);
    if (res != COAP_OK) {
        return res;
    }

    /* Notify resource about unlinking of the observer. */
    pRes->ObserverInfo(pObserver, false, pRes, pObserver->Ep.session);

    /* Remove observer from the resource. */
    res = CoAP_RemoveObserverFromResource(&pRes->pListObservers,
                                          pObserver->socketHandle,
                                          &pObserver->Ep,
                                          pObserver->Token);
    if (res != COAP_REMOVED) {
        return res;
    }
    return COAP_OK;
}


CoAP_Result_t _rom CoAP_handleDisconnectEvt(uint32_t transport_ctx) {
	CoAP_Observer_t *pObserver = NULL;
	CoAP_Res_t *pRes = NULL;
	CoAP_Result_t res = CoAP_FindObserverAndResourceByTransportCtx(transport_ctx,
																   &pObserver,
																   &pRes);
	if (res != COAP_OK) {
		return res;
	}

	/* Notify resource about unlinking of the observer. */
	pRes->ObserverInfo(pObserver, false, pRes, pObserver->Ep.session);

	/* Delete/abort any pending notification interaction. */
	res = CoAP_RemoveObserverInteractions(pObserver);
	if (res != COAP_OK) {
		return res;
	}

	/* Remove observer from the resource. */
	res = CoAP_RemoveObserverFromResource(&pRes->pListObservers,
										  pObserver->socketHandle,
										  &pObserver->Ep,
										  pObserver->Token);
	if (res != COAP_REMOVED) {
		return res;
	}
	return COAP_OK;
}

static CoAP_Result_t _rom SendResp(CoAP_Interaction_t* pIA, CoAP_InteractionState_t nextIAState) {
	CoAP_Result_t res = CoAP_SendMsg(pIA->pRespMsg, pIA->socketHandle, pIA->RemoteEp);
	if (res == COAP_OK) {

		if (pIA->pRespMsg->Type == ACK) { //piggy back resp
			pIA->ReqConfirmState = ACK_SEND;
		} else if (pIA->pRespMsg->Type == CON) {
			CoAP_EnableAckTimeout(pIA, pIA->RetransCounter); //enable timeout on waiting for ack
		} //else NON (no special handling)

		pIA->State = nextIAState; //move to next state

		CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing//todo: in die äußere statemachine

	} else { //unexspected internal failure todo: try at least to send 4 byte RESP_INTERNAL_SERVER_ERROR_5_00
		INFO("(!!!) SendResp(): Internal socket error on sending response! MiD: %d, CoapResult: %d", pIA->pRespMsg->MessageID, res);
		CoAP_DeleteInteraction(pIA);
		return COAP_ERR_SOCKET;
	}

	return COAP_OK;
}

static CoAP_Result_t _rom SendReq(CoAP_Interaction_t* pIA, CoAP_InteractionState_t nextIAState) {
	CoAP_Result_t res = CoAP_SendMsg(pIA->pReqMsg, pIA->socketHandle, pIA->RemoteEp);
	if (res == COAP_OK) {

		if (pIA->pReqMsg->Type == CON) {
			CoAP_EnableAckTimeout(pIA, pIA->RetransCounter); //enable timeout on waiting for ack
		} //else NON (no special handling=

		pIA->State = nextIAState; //move to next state

		CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing//todo: in die äußere statemachine

	} else { //unexspected internal failure todo: try at least to send 4 byte RESP_INTERNAL_SERVER_ERROR_5_00
		INFO("(!!!) SendReq(): Internal socket error on sending response! MiD: %d, CoapResult: %d\r\n", pIA->pReqMsg->MessageID, res);
		CoAP_DeleteInteraction(pIA);
		return COAP_ERR_SOCKET;
	}

	return COAP_OK;
}

//used on [server]
static CoAP_Result_t _rom CheckRespStatus(CoAP_Interaction_t* pIA) {

	if (pIA->ResConfirmState == RST_SEND) {
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
		if (pIA->ResConfirmState == ACK_SEND) { //everything fine!
			INFO("- Response ACKed by Client -> Transaction ended successfully\r\n");
			//todo: call success callback to user
			return COAP_OK;
		} else { //check ACK/RST timeout of our CON response
			if (timeAfter(CoAP.api.rtc1HzCnt(), pIA->AckTimeout)) {
				if (pIA->RetransCounter + 1 > MAX_RETRANSMIT) { //give up
					INFO("- (!) ACK timeout on sending response, giving up! Resp.MiD: %d\r\n",
							pIA->pRespMsg->MessageID);
					return COAP_ERR_OUT_OF_ATTEMPTS;
				} else {
					INFO("- (!) Retry num %d\r\n", pIA->RetransCounter + 1);
					return COAP_RETRY;
				}
			} else {
				pIA->SleepUntil = pIA->AckTimeout; // Let the interaction sleep till the ACK timeout
				return COAP_WAITING;
			}
		}
	}

	INFO("(!!!) CheckRespStatus(...) COAP_ERR_ARGUMENT !?!?\r\n");
	return COAP_ERR_ARGUMENT;
}

//used on [CLIENT] side request to check progress of interaction
static CoAP_Result_t _rom CheckReqStatus(CoAP_Interaction_t* pIA) {

	if (pIA->ReqConfirmState == RST_SEND) {
		INFO("- Response reset by remote server -> Interaction aborted\r\n");
		//todo: call failure callback to user
		return COAP_ERR_REMOTE_RST;
	}
        
	if (pIA->pReqMsg->Type == CON) { // We send a CON
		if (pIA->ReqConfirmState == ACK_SEND) {
			if (CoAP_MsgIsOlderThan(pIA->pReqMsg, CLIENT_MAX_RESP_WAIT_TIME)) {
				INFO("- Request ACKed separate by server, but giving up to wait for actual response data\r\n");
				return COAP_ERR_TIMEOUT;
			}

			INFO("- Request ACKed separate by server -> Waiting for actual response\r\n");
			return COAP_WAITING;
		} else { //check ACK/RST timeout of our CON request
			if (timeAfter(CoAP.api.rtc1HzCnt(), pIA->AckTimeout)) {
				if (pIA->RetransCounter + 1 > MAX_RETRANSMIT) { //give up
					INFO("- (!) ACK timeout on sending request, giving up! MiD: %d\r\n", pIA->pReqMsg->MessageID);
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
        
		if (CoAP_MsgIsOlderThan(pIA->pReqMsg, CLIENT_MAX_RESP_WAIT_TIME) && pIA->Role != COAP_ROLE_OBSERVATION) {
            
			INFO("- [NON request]: Giving up to wait for actual response data\r\n");
			return COAP_ERR_TIMEOUT;
		} else
			return COAP_WAITING;
	}

	INFO("(!!!) CheckReqStatus(...) COAP_ERR_ARGUMENT !?!?\r\n");
	return COAP_ERR_ARGUMENT;
}

CoAP_Socket_t* CoAP_NewSocket(SocketHandle_t handle) {
	CoAP_Socket_t* socket = AllocSocket();
	socket->Handle = handle;
	return socket;
}

CoAP_Result_t CoAP_RemoveSocket(CoAP_Socket_t *socket) {
	if (NULL == socket) {
		return COAP_ERR_ARGUMENT;
	}

	return FreeSocket(socket);
}

/**
 * Check if the response should be sent.
 * 
 * Based on No-Response option and message code, server can decide whether to send
 * response or not [RFC7967]. If OSCORE option is available in request message,
 * OSCORE library should handle No-Response option.
 * 
 * @param pIA		CoAP interaction.
 * @return true   	The response should be sent.
 * @return false  	The response should not be sent.
 */
static bool ShouldSendResponse( CoAP_Interaction_t* pIA )
{
	if( NULL != CoAP_FindOptionByNumber( pIA->pReqMsg, OPT_NUM_OSCORE ) )
	{
		// If message is OSCORE message, No-Response option should be handled by OSCORE library
		return true;
	}
	
	if( ACK == pIA->pRespMsg->Type )
	{
		// No-Response option does not apply for ACK messages
		return true;
	}
	CoAP_option_t * noResponseOption = CoAP_FindOptionByNumber( pIA->pReqMsg, OPT_NUM_NO_RESPONSE );
	if( NULL == noResponseOption )
	{
		return true;
	}

	uint32_t noResponseOptionValue = 0;
	if( COAP_OK != CoAP_GetUintFromOption(noResponseOption, &noResponseOptionValue) )
	{
		return true;
	}

	uint8_t messageClass = GET_CLASS_FROM_MESSAGE_CODE( pIA->pRespMsg->Code );

	// Granular control over response suppression (See section 2.1 of [RFC7967]).
	if ( ( ( MESSAGE_CLASS_2 == messageClass ) && ( NO_RESPONSE_FOR_CLASS_2 & noResponseOptionValue ) ) ||
			( ( MESSAGE_CLASS_4 == messageClass ) && ( NO_RESPONSE_FOR_CLASS_4 & noResponseOptionValue ) ) ||
			( ( MESSAGE_CLASS_5 == messageClass ) && ( NO_RESPONSE_FOR_CLASS_5 & noResponseOptionValue ) ) )
	{
		return false;
	}

	return true;
}

static void handleServerInteraction(CoAP_Interaction_t* pIA) {
	if (pIA->State == COAP_STATE_HANDLE_REQUEST ||
			pIA->State == COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT ||
			pIA->State == COAP_STATE_RESPONSE_WAITING_LEISURE) {

		if (pIA->ReqMetaInfo.Type == META_INFO_MULTICAST) {
			// Messages sent via multicast MUST be NON-confirmable.
			if (pIA->pReqMsg->Type == CON) {
				INFO("Request received from multicast endpoint is not allowed");
				CoAP_DeleteInteraction(pIA);
				return;
			}
			// Multicast messages get a response after a leisure period.
			if (pIA->State == COAP_STATE_HANDLE_REQUEST) {
				pIA->State = COAP_STATE_RESPONSE_WAITING_LEISURE;

				// Todo: Pick a random leisure period (See section 8.2 of [RFC7252])
				CoAP_SetSleepInteraction(pIA, DEFAULT_LEISURE); // Don't respond right away'
				CoAP_EnqueueLastInteraction(pIA);
				INFO("Multicast request postponed processing until %" PRIu32 "\r\n", pIA->SleepUntil);
				return;
			}
		}

		if (((pIA->pReqMsg->Code == REQ_GET) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_GET))
				|| ((pIA->pReqMsg->Code == REQ_POST) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_POST))
				|| ((pIA->pReqMsg->Code == REQ_PUT) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_PUT))
				|| ((pIA->pReqMsg->Code == REQ_DELETE) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_DELETE))
				|| ((pIA->pReqMsg->Code == REQ_FETCH) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_FETCH))
				|| ((pIA->pReqMsg->Code == REQ_PATCH) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_PATCH))
				|| ((pIA->pReqMsg->Code == REQ_IPATCH) && !((pIA->pRes->Options).AllowedMethods & RES_OPT_IPATCH))
				) {
			pIA->pRespMsg = CoAP_AllocRespMsg(pIA->pReqMsg, RESP_METHOD_NOT_ALLOWED_4_05, 0); //matches also TYPE + TOKEN to request

			if (ShouldSendResponse(pIA))
			{
				//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
				//transmit response & move to next state
				SendResp(pIA, COAP_STATE_RESPONSE_SENT);
				//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			}
			else
			{
				CoAP_DeleteInteraction(pIA);
			}
			return;
		}
		// (else) request method supported by resource...:

		// Allocate new msg with payload buffer which can be directly used OR overwritten
		// by resource handler with (ownstatic memory OR  "com_mem_get(...)" memory areas.
		// Any non static memory will be freed along with message! see free_Payload(...) function, even if user overwrites payload pointer!
		if (pIA->pRespMsg == NULL) { //if postponed before it would have been already allocated
			pIA->pRespMsg = CoAP_AllocRespMsg(pIA->pReqMsg, EMPTY, PREFERED_PAYLOAD_SIZE); //matches also TYPE + TOKEN to request
		}

		// Call of external set resource handler
		// could change type and code of message (ACK & EMPTY above only a guess!)
		CoAP_HandlerResult_t Res = HANDLER_ERROR;
		if((( ENC_END_POINT_ENC == pIA->pRes->Options.EncEndPoint ) && ( true == CoAP.api.is_session_valid( pIA->RemoteEp.session ))) ||
			(( ENC_END_POINT_COAP == pIA->pRes->Options.EncEndPoint ) && (NULL == pIA->RemoteEp.session )) ||
			( ENC_END_POINT_MIXED == pIA->pRes->Options.EncEndPoint ))
		{
			Res = pIA->pRes->Handler(pIA->pReqMsg, pIA->pRespMsg, pIA->RemoteEp.session);
		}
		else
		{
			pIA->pRespMsg->Code = RESP_METHOD_NOT_ACCEPTABLE_4_06; // TODO: verify if can jump to function end
		}

		// make sure the handler returned valid response (either already allocated OR allocated by handler itself)
		if (pIA->pRespMsg == NULL)
		{
			INFO("Resource handler returned NULLed response message (removing interaction)\r\n");
			CoAP_DeleteInteraction(pIA);
			return;
		}

		// Check return value of handler:
		// a) everything fine - we got an response to send
		if (Res == HANDLER_OK && pIA->pRespMsg->Code == EMPTY) {
			pIA->pRespMsg->Code = RESP_SUCCESS_CONTENT_2_05; //handler forgot to set code?

			// b) handler has no result and will not deliver	in the future
		} else if (Res == HANDLER_ERROR && pIA->pRespMsg->Code == EMPTY) {
			pIA->pRespMsg->Code = RESP_INTERNAL_SERVER_ERROR_5_00; //handler forgot to set code?

			// Don't respond with reset or empty messages to requests originating from multicast enpoints
			if (pIA->ReqMetaInfo.Type == META_INFO_MULTICAST) {
				CoAP_DeleteInteraction(pIA);
				return;
			}

			// c) handler needs some more time
		} else if (Res == HANDLER_POSTPONE) { // Handler needs more time to fulfill request, send ACK and separate response
			if (pIA->pReqMsg->Type == CON && pIA->ReqConfirmState != ACK_SEND) {
				if (CoAP_SendEmptyAck(pIA->pReqMsg->MessageID, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {

					pIA->ReqConfirmState = ACK_SEND;
					pIA->State = COAP_STATE_RESOURCE_POSTPONE_EMPTY_ACK_SENT;
					// give resource some time to become ready
					CoAP_SetSleepInteraction(pIA, POSTPONE_WAIT_TIME_SEK);

					CoAP_EnqueueLastInteraction(pIA);
					INFO("Resource not ready, postponed response until %" PRIu32 "\r\n", pIA->SleepUntil);
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
			if (pIA->ReqConfirmState ==
					ACK_SEND) { //separate empty ACK has been sent before (piggyback-ack no more possible)
				pIA->pRespMsg->Type = CON;
				pIA->pRespMsg->MessageID = CoAP_GetNextMid(); //we must use/generate a new messageID;
			} else
				pIA->pRespMsg->Type = ACK; //"piggybacked ack"
		}

		//Add custom option #10000 with hopcount and rssi of request to response
		if (pIA->ReqMetaInfo.Type == META_INFO_RF_PATH) {
			uint8_t buf_temp[2];
			buf_temp[0] = pIA->ReqMetaInfo.Dat.RfPath.HopCount;
			buf_temp[1] = pIA->ReqMetaInfo.Dat.RfPath.RSSI * -1;
			CoAP_AppendOptionToList(&(pIA->pRespMsg->pOptionsList), 10000, buf_temp, 2); //custom option #10000
		}

		//handle for GET observe option
		if ((pIA->pReqMsg->Code == REQ_GET || pIA->pReqMsg->Code == REQ_FETCH) && pIA->pRespMsg->Code == RESP_SUCCESS_CONTENT_2_05) {
			switch ( CoAP_HandleObservationInReq(pIA) )
			{
				case COAP_OK:
					AddObserveOptionToMsg(pIA->pRespMsg, 0);  //= ACK observation to client
					INFO("- Observation activated\r\n");
					break;
				case COAP_REMOVED:
					INFO("- Observation actively removed by client\r\n");
					break;
				case COAP_OBSERVE_NOT_FOUND:
					INFO("- No Observe option in request message\r\n");
					break;
				default:
					pIA->pRespMsg->Code = RESP_NOT_FOUND_4_04;
					INFO("- Observation failed\r\n");
					break;				
			}
		}

		//handle non sendable NON and response send cases. Left in the end intentionally due to possibile
		//GET request with NON type that starts observation
		if (COAP_RESP_NO_NON == pIA->pRes->Options.ResponseType && pIA->pReqMsg->Type == NON && pIA->pReqMsg->Code != REQ_GET) {
			CoAP_DeleteInteraction(pIA);
			return;
		}

		if( !ShouldSendResponse(pIA) )
		{
			CoAP_DeleteInteraction(pIA);
			return;
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
}

static void handleNotifyInteraction(CoAP_Interaction_t* pIA) {
	CoAP_Result_t res;
	CoAP_Observer_t* pObserver = NULL;
	if (pIA->State == COAP_STATE_READY_TO_NOTIFY) {
		//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		INFO("Sending Notification\n");
		SendResp(pIA, COAP_STATE_NOTIFICATION_SENT); //transmit response & move to next state
		//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		//--------------------------------------------------
	} else if (pIA->State == COAP_STATE_NOTIFICATION_SENT) {
		CoAP_Result_t respStatus = CheckRespStatus(pIA);
		INFO("Response Status: %s\n", ResultToString(respStatus));

		switch (respStatus) {
		case COAP_WAITING:
			CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing
			break;
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
				if (pIA->pRes->Notifier(pIA->pObserver, pIA->pRespMsg) == HANDLER_ERROR) {
					RemoveObserveOptionFromMsg(pIA->pRespMsg);
					CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token);
				} else { //good response
					UpdateObserveOptionInMsg(pIA->pRespMsg, pIA->pRes->UpdateCnt);
				}
				// The pIA->pRes->Notifier might have change the response from CON -> NON
				// On the interaction we like to preserve the original value
				// e.g. one CON between many NON messages should be preserved
				pIA->pRespMsg->Type = TypeSave;
			}
#endif

			if (CoAP_SendMsg(pIA->pRespMsg, pIA->socketHandle, pIA->RemoteEp) == COAP_OK) {
				pIA->RetransCounter++;
				CoAP_EnableAckTimeout(pIA, pIA->RetransCounter);
				INFO("- Changed notification body during retry\r\n");
			} else {
				INFO("(!!!) Internal socket error on sending response! MiD: %d\r\n", pIA->pRespMsg->MessageID);
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
				pIA->ResConfirmState = NOT_SET;

				//call notifier
				if (pIA->pRes->Notifier(pIA->pObserver, pIA->pRespMsg) == HANDLER_ERROR) {
					RemoveObserveOptionFromMsg(pIA->pRespMsg);
					CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token);
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
			res = CoAP_GetInteractionsObserver(pIA, &pObserver, pIA->pRespMsg->Token);
			if((COAP_OK == res) && (NULL != pIA->pRes->ObserverInfo)) {
				INFO("Abort of pending notificaton interaction\r\n");
				pIA->pRes->ObserverInfo(pObserver, false, pIA->pRes, pIA->RemoteEp.session);
			}
			CoAP_RemoveInteractionsObserver(pIA, pIA->pRespMsg->Token);  //remove observer from resource
			CoAP_DeleteInteraction(pIA);
			break;
		default:
			CoAP_DeleteInteraction(pIA);
			break;
		}
	}
}

static void handleClientInteraction(CoAP_Interaction_t* pIA) {

	//------------------------------------------
	if (pIA->State == COAP_STATE_READY_TO_REQUEST) {
		//------------------------------------------
		//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        if (pIA->Role == COAP_ROLE_OBSERVATION) {
            SendReq(pIA, COAP_STATE_WAITING_RESPONSE); //transmit response & move to next state
        }
        else if (pIA->Role == COAP_ROLE_CLIENT) {
            SendReq(pIA, COAP_STATE_WAITING_RESPONSE); //transmit response & move to next state
        }
		//o>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		//--------------------------------------------------
	} else if (pIA->State == COAP_STATE_WAITING_RESPONSE) {
		//--------------------------------------------------
		CoAP_Result_t reqStatus = CheckReqStatus(pIA);
		// INFO("Request Status: %s\n", ResultToString(reqStatus));
		switch (reqStatus) {
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
				if (pIA->RespCB != NULL) {
                    DEBUG("Calling RespCB for pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pRespMsg->Code, pIA->pRespMsg->Type, pIA->Role);
					pIA->RespCB(NULL, pIA->pReqMsg, &pIA->RemoteEp);
				}
				CoAP_DeleteInteraction(pIA);
			}
			break;

		case COAP_ERR_OUT_OF_ATTEMPTS: //check is resource is a lazy observe delete one
		case COAP_ERR_REMOTE_RST:
		case COAP_ERR_TIMEOUT:
		default:
			if (pIA->RespCB != NULL) {
				pIA->RespCB(NULL, pIA->pReqMsg, &pIA->RemoteEp);
			}
			CoAP_DeleteInteraction(pIA);
		}
    } else if (pIA->State == COAP_STATE_WAITING_NOTIFICATION) {
        
        CoAP_EnqueueLastInteraction(pIA); //(re)enqueue interaction for further processing
        
    } else if (pIA->State == COAP_STATE_HANDLE_RESPONSE) {
        //--------------------------------------------------
        DEBUG("- Got Response to Client request! -> calling Handler!\r\n");
        if (pIA->RespCB != NULL) {
            DEBUG("Calling RespCB for pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pRespMsg->Code, pIA->pRespMsg->Type, pIA->Role);
            pIA->RespCB(pIA->pRespMsg, pIA->pReqMsg, &(pIA->RemoteEp)); //call callback
        }
        
        CoAP_DeleteInteraction(pIA); //direct delete, todo: eventually wait some time to send ACK instead of RST if out ACK to remote reponse was lost

        //--------------------------------------------------
	} else if (pIA->State == COAP_STATE_HANDLE_NOTIFICATION) {
		//--------------------------------------------------
		DEBUG("- Got Notification! -> calling Handler!\r\n");
		if (pIA->RespCB != NULL) {
            DEBUG("Calling RespCB for pIA Req %p, MessageID: %d, Code: %d, Type: %d; Role: %d\n", pIA, pIA->pReqMsg->MessageID, pIA->pRespMsg->Code, pIA->pRespMsg->Type, pIA->Role);
			pIA->RespCB(pIA->pRespMsg, pIA->pReqMsg, &(pIA->RemoteEp)); //call callback
            
		}
        // If there is an error code in Response Message remove Interaction
        if (pIA->pRespMsg->Code > CODE(4u, 0u)) {
            CoAP_DeleteInteraction(pIA);
        }
        // Wait for more notifications
        else {
            pIA->State = COAP_STATE_WAITING_NOTIFICATION;
        }
	} else {
		if (pIA->RespCB != NULL) {
			pIA->RespCB(NULL, pIA->pReqMsg, &pIA->RemoteEp);
		}
		CoAP_DeleteInteraction(pIA); //unknown state, should not go here
	}
}

uint32_t _rom CoAP_getNextInteractionTime( void )
{
	CoAP_Interaction_t* pIA = CoAP.pInteractions;
	uint32_t nextInteractionTime = COAP_WAIT_FOREVER;
	while (NULL != pIA) {
		if (pIA->SleepUntil < nextInteractionTime) {
			nextInteractionTime = pIA->SleepUntil;
		}
		pIA = pIA->next;
	}
	return nextInteractionTime;
}

//must be called regularly
void _rom CoAP_doWork() {
	CoAP_Interaction_t* pIA = CoAP_GetLongestPendingInteraction();

	if (pIA == NULL) {
		//nothing to do now
		return;
	}



	uint32_t now = CoAP.api.rtc1HzCnt();

	if (timeAfter(pIA->SleepUntil, now)) {
		//INFO("Interaction paused till %lu (remaining: %lu)\n",pIA->SleepUntil, pIA->SleepUntil - now);
		CoAP_EnqueueLastInteraction(pIA);
		return;
	}

	// DEBUG output all interactions
	//INFO("\n\r");
	//PrintInteractions(CoAP.pInteractions);
	//coap_mem_stats();

	//INFO("Now: %lu\n", now);

	//	INFO("pending Transaction found! ReqTime: %u\r\n", pIA->ReqTime);
	//	com_mem_stats();

	switch (pIA->Role) {
	case COAP_ROLE_SERVER:
		handleServerInteraction(pIA);
		break;
	case COAP_ROLE_NOTIFICATION:
		handleNotifyInteraction(pIA);
		break;
	case COAP_ROLE_OBSERVATION:
    case COAP_ROLE_CLIENT:
		handleClientInteraction(pIA);
		break;
	default:
		ERROR("Unknown Notification Role: %d", pIA->Role);
	}
}

void _rom CoAP_ClearPendingInteractions() {
    CoAP_ClearInteractions(&CoAP.pInteractions);
}
