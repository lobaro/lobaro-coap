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
#include <inttypes.h>

static CoAP_Res_t* pResList = NULL;
static uint32_t ResListMembers = 0;

uint8_t TempPage[2048];


/**
 * Save resource and observe information as options to non volatile storage
 *
 * @param writeBufFn a function that can write bytes to non volatile memory
 * @return
 */
CoAP_Result_t _rom CoAP_NVsaveObservers(WriteBuf_fn writeBufFn) {
	CoAP_Res_t* pList = pResList; //List of internal resources
	CoAP_option_t* pOptList = NULL;
	uint8_t* pTempPage = TempPage;
	uint32_t TotalPageBytes = 0;
	bool UriPathHasBeenEncoded = false; //uri path options are only encoded one time for each resource and not for every observer

	while (pList != NULL) { //iterate over all resources
		UriPathHasBeenEncoded = false; //reset for next resource

		if (pList->pListObservers != NULL) { //Resource has active observers

			CoAP_Observer_t* pObserverList = pList->pListObservers;

			while (pObserverList != NULL) { //iterate over all observers of this resource

				//Store Uri of external Oberserver as option
				CoAP_AppendOptionToList(&pOptList, OPT_NUM_URI_HOST, (uint8_t*) &(pObserverList->Ep), sizeof(NetEp_t)); //IP+Port of external Observer
				CoAP_AppendOptionToList(&pOptList, OPT_NUM_URI_PORT, (uint8_t*) &(pObserverList->socketHandle), sizeof(SocketHandle_t)); //socketHandle as pseudo "Port"

				CoAP_option_t* pOptionsTemp = pList->pUri;

				//Copy uri-path option of resource
				if (UriPathHasBeenEncoded == false) {
					while (pOptionsTemp != NULL) {
						CoAP_CopyOptionToList(&pOptList, pOptionsTemp); //todo check for sufficent memory or implement cross linking to save memory
						pOptionsTemp = pOptionsTemp->next;
					}
					UriPathHasBeenEncoded = true; //store uri path of resource only once
				}

				//Also copy any other options of observe structure (e.g. uri-query)
				pOptionsTemp = pObserverList->pOptList;
				while (pOptionsTemp != NULL) {
					CoAP_CopyOptionToList(&pOptList, pOptionsTemp); //todo check for sufficent memory or implement cross linking to save memory
					pOptionsTemp = pOptionsTemp->next;
				}

				CoAP_AppendOptionToList(&pOptList, OPT_NUM_LOBARO_TOKEN_SAVE, (uint8_t*) &(pObserverList->Token), 8);

				uint16_t BytesWritten = 0;
				pack_OptionsFromList(pTempPage, &BytesWritten, pOptList);

				pTempPage[BytesWritten] = 0xff; //add pseudo "payload"-marker to make reparsing easier
				TotalPageBytes += BytesWritten + 1;
				pTempPage = pTempPage + TotalPageBytes;

				INFO("Wrote Observer Option List:\r\n");
				CoAP_printOptionsList(pOptList);

				CoAP_FreeOptionList(&pOptList); //free options
				pObserverList = pObserverList->next;
			}
		}

		pList = pList->next;
	}

	pTempPage = TempPage;
	INFO("writing: %"PRIu32" bytes to flash\r\n", TotalPageBytes);
	writeBufFn(pTempPage, TotalPageBytes);
	return COAP_OK;
}

/**
 * Load and attach observers
 * @param pRawPage pointer to the non volatile memory to load the observer from
 * @return
 */
CoAP_Result_t _rom CoAP_NVloadObservers(uint8_t* pRawPage) {
	CoAP_option_t* pOptList = NULL;
	CoAP_Res_t* pRes = NULL;
	CoAP_Res_t* pResTemp = NULL;

	while (parse_OptionsFromRaw(pRawPage, 2048, &pRawPage, &pOptList) == COAP_OK) { //finds "payload-marker" and sets pointer to its beginning. in this context this is the next stored observe dataset
		INFO("found flash stored options:\r\n");
		CoAP_printOptionsList(pOptList);

		//Map Stored option to resource
		pResTemp = CoAP_FindResourceByUri(pResList, pOptList);
		if (pResTemp != NULL) pRes = pResTemp;
		else {
			INFO("- Observed Resource not found!\r\n");
			//todo: del observe res?
			continue;
		}


		//(re)create observer for resource
		CoAP_Observer_t* pNewObserver = CoAP_AllocNewObserver();

		if (pNewObserver == NULL) {
			INFO("pNewObserver out of Mem!\r\n");
			//todo: do anything different to simple continue
			continue;
		}

		CoAP_option_t* pOpt = pOptList;
		while (pOpt != NULL) {
			INFO(".");

			switch (pOpt->Number) {
				case OPT_NUM_URI_HOST:
					coap_memcpy((void*) &(pNewObserver->Ep), pOpt->Value, sizeof(NetEp_t));
					break;
				case OPT_NUM_URI_PORT: //"hack" netID, remove if netID removal cleanup done!
					coap_memcpy((void*) &(pNewObserver->socketHandle), pOpt->Value, sizeof(SocketHandle_t));
					break;
				case OPT_NUM_URI_PATH:
					break; //dont copy path to observe struct (it's connected to its resource anyway!)
				case OPT_NUM_LOBARO_TOKEN_SAVE:
					coap_memcpy((void*) &(pNewObserver->Token), pOpt->Value, 8);
					break;
				default:
					CoAP_CopyOptionToList(&(pNewObserver->pOptList), pOpt);
					break;
			}

			pOpt = pOpt->next;
		}

		//attach observer to resource
		CoAP_AppendObserverToList(&(pRes->pListObservers), pNewObserver);

		CoAP_FreeOptionList(&pOptList); //free temp options
	}

	CoAP_PrintAllResources();
	//CoAP_FindResourceByUri
	return COAP_OK;
}

/**
 * @brief Safely appends a single character to the buffer at given index.
 * 
 * @param buffer buffer to be modified
 * @param bufferSize buffer size in bytes
 * @param index position inside the buffer
 * @param character data to be appended
 * @return true if appended correctly
 * @return false if new data would be out of bands
 */
static bool appendChar(uint8_t* buffer, size_t bufferSize, size_t* index, char character)
{
	//check if character would fit in the output buffer
	//last character of the output buffer is reserved for null termination
	if (*index >= (bufferSize - 1))
	{
		return false;
	}

	buffer[*index] = (uint8_t)character;
	(*index)++;
	return true;
}

/**
 * @brief Safely appends a sub-buffer to the buffer starting at given index.
 * 
 * @param buffer buffer to be modified
 * @param bufferSize buffer size in bytes
 * @param index starting position inside the buffer
 * @param subBuffer data buffer to be appended
 * @param subBufferSize size of data buffer in bytes
 * @return true if appended correctly
 * @return false if new data would be out of bands
 */
static bool appendBuffer(uint8_t* buffer, size_t bufferSize, size_t* index, const uint8_t* subBuffer, size_t subBufferSize)
{
	//check if subbuffer would fit in the output buffer
	//last character of the output buffer is reserved for null termination; -1 on both sides left for clarity
	if (((*index) + subBufferSize - 1) >= (bufferSize - 1))
	{
		return false;
	}

	coap_memcpy(&buffer[*index], subBuffer, subBufferSize);
	*index += subBufferSize;
	return true;
}

/**
 * @brief Safely appends a string to the buffer starting at given index.
 * 
 * @param buffer buffer to be modified
 * @param bufferSize buffer size in bytes
 * @param index starting position inside the buffer
 * @param string string buffer to be appended (must end with null termination)
 * @return true if appended correctly
 * @return false if new data would be out of bands
 */
static bool appendString(uint8_t* buffer, size_t bufferSize, size_t* index, const char* string)
{
	size_t length = strlen(string);
	return appendBuffer(buffer, bufferSize, index, (const uint8_t*)string, length);
}

CoAP_HandlerResult_t _rom WellKnown_GetHandler(CoAP_Message_t* pReq, CoAP_Message_t* pResp, void *c_ctx) {
	(void)c_ctx;

	if (pReq->Code != REQ_GET) {
		uint8_t errMsg[] = {"CoAP GET only!"};
		pResp->Code = RESP_ERROR_BAD_REQUEST_4_00;
		CoAP_SetPayload(pResp, errMsg, (uint16_t) (sizeof(errMsg)-1), true);
		return HANDLER_ERROR;
	}

	CoAP_Res_t* pList = pResList; //List of internal resources
	size_t allocatedSize = (ResListMembers + 1) * 64; //first estimation of needed memory
	size_t currentSize = 0;
	uint8_t* pStr = (uint8_t*) CoAP.api.malloc(allocatedSize);

	if (pStr == NULL) {
		INFO("- WellKnown_GetHandler(): Ouf memory error!\r\n");
		return HANDLER_ERROR;
	}
	memset(pStr, 0, allocatedSize);

	INFO("- WellKnown_GetHandler(): res cnt:%u temp alloc:%u\r\n", (unsigned int) ResListMembers, (unsigned int) allocatedSize);

	//TODO: Implement non ram version, e.g. write to memory to eeprom
	bool isBufferOk = true;
	while ((isBufferOk) && (pList != NULL)) {
		CoAP_option_t* pUriOpt = pList->pUri;

		isBufferOk &= appendChar(pStr, allocatedSize, &currentSize, '<');
		while (pUriOpt != NULL) {
			isBufferOk &= appendChar(pStr, allocatedSize, &currentSize, '/');
			isBufferOk &= appendBuffer(pStr, allocatedSize, &currentSize, pUriOpt->Value, pUriOpt->Length);
			pUriOpt = pUriOpt->next;
		}
		isBufferOk &= appendChar(pStr, allocatedSize, &currentSize, '>');
		if (pList->Options.Cf != COAP_CF_LINK_FORMAT) {
			if( pList->pDescription != NULL ) {
				isBufferOk &= appendString(pStr, allocatedSize, &currentSize, ";title=\"");
				isBufferOk &= appendString(pStr, allocatedSize, &currentSize, pList->pDescription);
				isBufferOk &= appendChar(pStr, allocatedSize, &currentSize, '\"');
			}
			
			char contentFormat[6]; //the field is uint16_t (up to 5 digits) + 1 byte for null termination
			coap_sprintf(contentFormat, "%d", pList->Options.Cf);
			isBufferOk &= appendString(pStr, allocatedSize, &currentSize, ";ct=");
			isBufferOk &= appendString(pStr, allocatedSize, &currentSize, contentFormat);
			
			if (pList->Notifier != NULL) {
				isBufferOk &= appendString(pStr, allocatedSize, &currentSize, ";obs");
			}
		}

		if(ENC_END_POINT_ENC == pList->Options.EncEndPoint){
			isBufferOk &= appendString(pStr, allocatedSize, &currentSize, ";osc");
		}

		isBufferOk &= appendChar(pStr, allocatedSize, &currentSize, ',');

		pList = pList->next;
	}

	if (!isBufferOk)
	{
		ERROR("Wellknown buffer too small, the output is truncated.");
	}

	CoAP_SetPayload(pResp, pStr, (uint16_t) coap_strlen((char*) pStr), true);
	CoAP.api.free(pStr);

	CoAP_AddCfOptionToMsg(pResp, COAP_CF_LINK_FORMAT);

	return HANDLER_OK;
}

void _rom CoAP_InitResources() {
	CoAP_ResOpts_t Options = {.Cf = COAP_CF_LINK_FORMAT, .AllowedMethods = RES_OPT_GET};
	CoAP_CreateResource("/.well-known/core", "\0", Options, WellKnown_GetHandler, NULL, NULL);
}

static CoAP_Result_t _rom CoAP_AppendResourceToList(CoAP_Res_t** pListStart, CoAP_Res_t* pResToAdd) {
	if (pResToAdd == NULL) return COAP_ERR_ARGUMENT;

	if (*pListStart == NULL) //List empty? create new first element
	{
		*pListStart = pResToAdd;
		(*pListStart)->next = NULL;
	} else //append new element at end
	{
		CoAP_Res_t* pRes = *pListStart;
		while (pRes->next != NULL) pRes = pRes->next;

		pRes->next = pResToAdd;
		pRes = pRes->next;
		pRes->next = NULL;
	}
	return COAP_OK;
}

CoAP_Result_t _rom CoAP_FreeResource(CoAP_Res_t **pResource) {
    CoAP_FreeOptionList(&(*pResource)->pUri);
    while (NULL != (*pResource)->pListObservers) {
        CoAP_UnlinkObserverFromList(&(*pResource)->pListObservers, (*pResource)->pListObservers, true);
    }

    CoAP.api.free((*pResource)->pDescription);
    CoAP.api.free((void*) (*pResource));
    *pResource = NULL;
    return COAP_OK;
}

static CoAP_Result_t CoAP_UnlinkResourceFromList(CoAP_Res_t **pListStart, CoAP_Res_t *pResToRemove, bool FreeUnlinked) {
    CoAP_Res_t *currP;
    CoAP_Res_t *prevP;

    // For 1st node, indicate there is no previous.
    prevP = NULL;

    //Visit each node, maintaining a pointer to
    //the previous node we just visited.
    for (currP = *pListStart; currP != NULL; prevP = currP, currP = currP->next) {

        if (currP == pResToRemove) { // Found it.
            if (prevP == NULL) {
                //Fix beginning pointer.
                *pListStart = currP->next;
            } else {
                //Fix previous node's next to
                //skip over the removed node.
                prevP->next = currP->next;
            }

            // Deallocate the node.
            if (FreeUnlinked) {
                CoAP_FreeResource(&currP);
            }
            //Done searching.
            ResListMembers--;
            return COAP_OK;
        }
    }
    return COAP_ERR_NOT_FOUND;
}

CoAP_Result_t _rom CoAP_RemoveResource(CoAP_Res_t *pResource) {
    if (NULL == pResource) {
        return COAP_ERR_ARGUMENT;
    }
    return CoAP_UnlinkResourceFromList(&pResList, pResource, true);
}

static CoAP_Result_t CoAP_UpdateResourceInList(CoAP_Res_t **pListStart, CoAP_Res_t *pResToUpdate, CoAP_ResOpts_t options) {
    CoAP_Res_t *currP;

    //Visit each node, maintaining a pointer to
    //the previous node we just visited.
    for (currP = *pListStart; currP != NULL; currP = currP->next) {

        if (currP == pResToUpdate) { // Found it.
			currP->Options.ResponseType = options.ResponseType;
			currP->Options.NotificationType = options.NotificationType;
            return COAP_OK;
        }
    }
    return COAP_ERR_NOT_FOUND;
}

CoAP_Result_t _rom CoAP_UpdateResource(CoAP_Res_t *pResource, CoAP_ResOpts_t options) {
    if (NULL == pResource) {
        return COAP_ERR_ARGUMENT;
    }
	return CoAP_UpdateResourceInList(&pResList, pResource, options);
}

CoAP_Res_t* _rom CoAP_FindResourceByUri(CoAP_Res_t* pResListToSearchIn, CoAP_option_t* pOptionsToMatch) {
	CoAP_Res_t* pList = pResList;
	if (pResListToSearchIn != NULL) {
		pList = pResListToSearchIn;
	}

	for (; pList != NULL; pList = pList->next) {
		if (CoAP_UriOptionsAreEqual(pList->pUri, pOptionsToMatch)) {
			return pList;
		}
	}

	return NULL;
}

CoAP_Res_t* _rom CoAP_CreateResource(const char* Uri, const char* Descr, CoAP_ResOpts_t Options,
        CoAP_ResourceHandler_fPtr_t pHandlerFkt, CoAP_ResourceNotifier_fPtr_t pNotifierFkt, CoAP_ResourceObserverInfo_t pObserverInfo) {
	INFO("Creating resource %s (%s) AllowedMethods: %x%x%x%x\r\n", Uri, Descr == NULL ? "" : Descr,
		 !!(Options.AllowedMethods & RES_OPT_GET),
		 !!(Options.AllowedMethods & RES_OPT_POST),
		 !!(Options.AllowedMethods & RES_OPT_PUT),
		 !!(Options.AllowedMethods & RES_OPT_DELETE));

	if (Options.AllowedMethods == 0) {
		ERROR("Can not create Resource that does not allow any method!");
		return NULL;
	}

	CoAP_Res_t* pRes = (CoAP_Res_t*) (CoAP.api.malloc(sizeof(CoAP_Res_t)));
	if (pRes == NULL) {
		return NULL;
	}
	memset(pRes, 0, sizeof(CoAP_Res_t));

	pRes->pListObservers = NULL;
	pRes->pUri = NULL;
	pRes->next = NULL;

	pRes->Options = Options;

	if (Descr != NULL && *Descr != '\0') {
		pRes->pDescription = (char*) (CoAP.api.malloc(sizeof(char) * (coap_strlen(Descr) + 1)));
		coap_strcpy(pRes->pDescription, Descr);
	} else {
		pRes->pDescription = NULL;
	}

	CoAP_AppendUriOptionsFromString(&(pRes->pUri), Uri);

	pRes->Handler = pHandlerFkt;
	pRes->Notifier = pNotifierFkt;
	pRes->ObserverInfo = pObserverInfo;

	CoAP_AppendResourceToList(&pResList, pRes);

	ResListMembers++;

	return pRes;
}


CoAP_Result_t _rom CoAP_NotifyResourceObservers(CoAP_Res_t* pRes) {
	pRes->UpdateCnt++;
	CoAP_StartNotifyInteractions(pRes); //async start of update interaction
	return COAP_OK;
}


void _rom CoAP_PrintResource(CoAP_Res_t* pRes) {
	CoAP_printUriOptionsList(pRes->pUri);
	INFO("Observers:\r\n");
	CoAP_Observer_t* pOpserver = pRes->pListObservers; //point to ListStart
	while (pOpserver != NULL) {
		INFO("Token (%"PRIu8"): %016"PRIx64" - ", pOpserver->Token.Length, (uint64_t)pOpserver->Token.Token[0]);
		PrintEndpoint(&(pOpserver->Ep));
		INFO("\n\r");
		CoAP_printUriOptionsList(pOpserver->pOptList);
		CoAP_printOptionsList(pOpserver->pOptList);
		INFO("---\r\n");
		pOpserver = pOpserver->next;

	}
	INFO("\r\n");
}

void _rom CoAP_PrintAllResources() {
	CoAP_Res_t* pRes = pResList;
	while (pRes != NULL) {
		CoAP_PrintResource(pRes);
		pRes = pRes->next;
	}
}


CoAP_Result_t _rom CoAP_RemoveObserverFromResource(CoAP_Observer_t** pObserverList, SocketHandle_t socketHandle, NetEp_t* pRemoteEP, CoAP_Token_t token) {
	CoAP_Observer_t* pObserver = *pObserverList;

	while (pObserver != NULL) { //found right existing observation -> delete it

		if (CoAP_TokenEqual(token, pObserver->Token) && socketHandle == pObserver->socketHandle && EpAreEqual(pRemoteEP, &(pObserver->Ep))) {

			INFO("- (!) Unlinking observer from resource\r\n");
			CoAP_UnlinkObserverFromList(pObserverList, pObserver, true);
			return COAP_REMOVED;

		}
		pObserver = pObserver->next;
	}
	return COAP_ERR_NOT_FOUND;
}


CoAP_Result_t _rom CoAP_MatchObserverFromList(CoAP_Observer_t** pObserverList, CoAP_Observer_t** pMatchingObserver, SocketHandle_t socketHandle, NetEp_t* pRemoteEP, CoAP_Token_t token) {
	CoAP_Observer_t* pObserver = *pObserverList;

	while (pObserver != NULL) { //found right existing observation -> get it

		if (CoAP_TokenEqual(token, pObserver->Token) && socketHandle == pObserver->socketHandle && EpAreEqual(pRemoteEP, &(pObserver->Ep))) {

			*pMatchingObserver = pObserver;
			return COAP_OK;

		}
		pObserver = pObserver->next;
	}
	return COAP_OBSERVE_NOT_FOUND;
}

CoAP_Result_t _rom CoAP_MatchObserverUsingTransportCtxFromList(CoAP_Observer_t **pObserverList,
															   CoAP_Observer_t **pMatchingObserver,
															   uint32_t transport_ctx) {
	CoAP_Observer_t *pObserver = *pObserverList;

	while (pObserver != NULL) { //found right existing observation -> get it

		if (pObserver->Ep.transport_ctx == transport_ctx) {
			*pMatchingObserver = pObserver;
			return COAP_OK;

		}
		pObserver = pObserver->next;
	}
	return COAP_ERR_NOT_FOUND;
}


CoAP_Result_t _rom CoAP_FindObserverAndResourceByTransportCtx(uint32_t transport_ctx,
															  CoAP_Observer_t **pObserver,
															  CoAP_Res_t **pRes) {
	CoAP_Result_t res;
	CoAP_Res_t *pList = pResList;

	for (; pList != NULL; pList = pList->next) {
		/* Iterate over all observers in the resource. */
		if (pList->pListObservers) {
			res = CoAP_MatchObserverUsingTransportCtxFromList(&pList->pListObservers,
															  pObserver,
															  transport_ctx);

			if (res == COAP_OK) {
				*pRes = pList;
				return res;
			}
		}
	}

	return COAP_ERR_NOT_FOUND;
}

CoAP_Result_t _rom CoAP_UninitResources() {
    CoAP_Result_t retval = COAP_ERR_NOT_FOUND;
    while (NULL != pResList) {
        retval = CoAP_UnlinkResourceFromList(&pResList, pResList, true);
        if (COAP_OK != retval) {
            return retval;
        }
    }
    return COAP_OK;
}
