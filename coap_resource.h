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
#ifndef SRC_COAP_COAP_RES_H_
#define SRC_COAP_COAP_RES_H_

typedef enum {
	HANDLER_OK =0,
	HANDLER_POSTPONE = 1,
	HANDLER_ERROR = 2
}CoAP_HandlerResult_t;

typedef CoAP_HandlerResult_t (*CoAP_ResourceHandler_fPtr_t)(CoAP_Message_t* pReq, CoAP_Message_t* pResp);
typedef CoAP_HandlerResult_t (*CoAP_ResourceNotifier_fPtr_t)(CoAP_Observer_t* pListObservers, CoAP_Message_t* pResp);

typedef uint32_t (*CoAP_ResourceGetETag_fPtr_t)();

//Bitfields for resource BitOpts
#define RES_OPT_GET 	(1<<REQ_GET) //1<<1
#define RES_OPT_POST 	(1<<REQ_POST) //1<<2
#define RES_OPT_PUT		(1<<REQ_PUT) //1<<3
#define RES_OPT_DELETE 	(1<<REQ_DELETE)//1<<4

typedef struct {
	uint16_t Cf; //Content-Format
	uint16_t Flags; //Bitwise resource options //todo: Send Response as CON or NON
	uint16_t ETag;
}CoAP_ResOpts_t;

struct CoAP_Res
{
	struct CoAP_Res* next; //4 byte pointer (linked list)
	char* pDescription;
	uint32_t UpdateCnt;
	CoAP_ResOpts_t Options;
	CoAP_option_t* pUri; //linked list of this resource URI options
	CoAP_Observer_t* pListObservers; //linked list of this resource observers
	CoAP_ResourceHandler_fPtr_t Handler;
	CoAP_ResourceNotifier_fPtr_t Notifier; //maybe "NULL" if resource not observable
};

typedef struct CoAP_Res CoAP_Res_t;

CoAP_Res_t* CoAP_CreateResource(char* Uri, char* Descr,CoAP_ResOpts_t Options, CoAP_ResourceHandler_fPtr_t pHandlerFkt, CoAP_ResourceNotifier_fPtr_t pNotifierFkt );
CoAP_Res_t* CoAP_FindResourceByUri(CoAP_Res_t* pResListToSearchIn, CoAP_option_t* pUriToMatch);
CoAP_Result_t CoAP_NotifyResourceObservers(CoAP_Res_t* pRes);
CoAP_Result_t CoAP_FreeResource(CoAP_Res_t** pResource);

void CoAP_PrintResource(CoAP_Res_t* pRes);
void CoAP_PrintAllResources();

void CoAP_InitResources();

CoAP_Result_t CoAP_NVsaveObservers();
CoAP_Result_t CoAP_NVloadObservers();

CoAP_Result_t CoAP_RemoveObserverFromResource(CoAP_Observer_t** pObserverList, uint8_t IfID, NetEp_t* pRemoteEP, uint64_t token);

#endif /* SRC_COAP_COAP_RESOURCES_H_ */
