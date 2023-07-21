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

#ifndef LIBLOBARO_COAP_H
#define LIBLOBARO_COAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

//################################
// Function Results
//################################

#define COAP_WAIT_FOREVER UINT32_MAX

typedef enum {
	COAP_OK = 0,
	COAP_NOT_FOUND, //not found but no error
	COAP_OBSERVE_NOT_FOUND, //Observe not found but no error
	COAP_PARSE_DATAGRAM_TOO_SHORT,
	COAP_PARSE_UNKOWN_COAP_VERSION,
	COAP_PARSE_MESSAGE_FORMAT_ERROR,
	COAP_PARSE_TOO_MANY_OPTIONS,
	COAP_PARSE_TOO_LONG_OPTION,
	COAP_PARSE_TOO_MUCH_PAYLOAD,
	COAP_PACK_TOO_MANY_OPTIONS,
	COAP_PACK_TOO_LONG_OPTION,
	COAP_ERR_ARGUMENT,
	COAP_ERR_SOCKET,
	COAP_ERR_NETWORK,
	COAP_ERR_OUT_OF_MEMORY,
	COAP_ERR_TOO_LONG_URI_PATH,
	COAP_ERR_NOT_FOUND,
	COAP_ERR_WRONG_OPTION,
	COAP_ERR_EXISTING,
	COAP_TRUE,
	COAP_FALSE,
	COAP_ERR_WRONG_REQUEST,
	COAP_BAD_OPTION_VAL,
	COAP_BAD_OPTION_LEN,
	COAP_REMOVED,
	COAP_ERR_UNKNOWN,
	COAP_ERR_REMOTE_RST,
	COAP_ERR_OUT_OF_ATTEMPTS,
	COAP_ERR_TIMEOUT,
	COAP_WAITING,
	COAP_HOLDING_BACK,
	COAP_RETRY
} CoAP_Result_t;

//################################
// Endpoints
//################################

//IPv6 address
typedef union {
	uint8_t u8[16];
	uint16_t u16[8];
	uint32_t u32[4];
} NetAddr_IPv6_t;

//IPv4 address
typedef union {
	uint8_t u8[4];
	uint16_t u16[2];
	uint32_t u32[1];
} NetAddr_IPv4_t;

//UART address
typedef struct {
	uint8_t ComPortID;
} NetAddr_Uart_t;

//general address
#define NetAddr_MAX_LENGTH (16)
typedef union {
	NetAddr_IPv6_t IPv6;
	NetAddr_IPv4_t IPv4;
	NetAddr_Uart_t Uart;
	uint8_t mem[NetAddr_MAX_LENGTH]; //used for init and comparisons of addresses
} NetAddr_t;

typedef enum {
	EP_NONE, IPV6, IPV4, BTLE, UART, USB
} NetInterfaceType_t;

// general network endpoint
// used by drivers and network libs
typedef struct {
	NetInterfaceType_t NetType;
	NetAddr_t NetAddr;
	uint16_t NetPort;
	void *session;
	uint32_t transport_ctx;
} NetEp_t;

//################################
// Packets
//################################

typedef enum {
	META_INFO_NONE,
	META_INFO_RF_PATH,
	META_INFO_MULTICAST
} MetaInfoType_t;

typedef struct {
	uint8_t HopCount;
	int32_t RSSI;
} MetaInfo_RfPath_t;

typedef union {
	MetaInfo_RfPath_t RfPath;
} MetaInfoUnion_t;

typedef struct {
	MetaInfoType_t Type;
	MetaInfoUnion_t Dat;
} MetaInfo_t;

// general network packet
// received in callbacks and send out
// in network send routines
typedef struct {
	uint8_t *pData;
	uint16_t size;
	// The remote EndPoint is either the sender for incoming packets or the receiver for outgoing packets
	NetEp_t remoteEp;
	// Optional meta info that will be translated into in options
	MetaInfo_t metaInfo;
} NetPacket_t;

//################################
// Sockets
//################################
typedef void *SocketHandle_t;

typedef void (*NetReceiveCallback_fn)(SocketHandle_t socketHandle, NetPacket_t *pckt);
typedef bool (*NetTransmit_fn)(SocketHandle_t socketHandle, NetPacket_t *pckt);

typedef struct {
	SocketHandle_t Handle; // Handle to identify the socket

	NetTransmit_fn Tx;     // ext. function called by coap stack to send data after finding socket by socketHandle (internally)
	bool Alive;            // We can only deal with sockets that are alive
} CoAP_Socket_t;

//################################
// Options
//################################

typedef enum {
	// Core Options
	OPT_NUM_URI_PATH = 11,
	OPT_NUM_URI_HOST = 3,
	OPT_NUM_ETAG = 4,
	OPT_NUM_OBSERVE = 6,
	OPT_NUM_URI_PORT = 7,
    OPT_NUM_OSCORE = 9,
	OPT_NUM_CONTENT_FORMAT = 12,
	OPT_NUM_URI_QUERY = 15,
	OPT_NUM_ACCEPT = 17,
	// Blockwise transfers
	OPT_NUM_BLOCK2 = 23,
	OPT_NUM_BLOCK1 = 27,
	OPT_NUM_SIZE2 = 28,
	OPT_NUM_SIZE1 = 60,
	// Freshness control
	OPT_NUM_ECHO = 252,
	OPT_NUM_NO_RESPONSE = 258,
	// Others
	OPT_NUM_LOBARO_TOKEN_SAVE = 350
} CoAP_KnownOptionNumbers_t;

typedef struct CoAP_option {
	struct CoAP_option *next; //4 byte pointer (linked list)

	uint16_t Number; //2 byte
	uint16_t Length; //2 byte
	uint8_t *Value;  //4 byte (should be last in struct!)
} CoAP_option_t;

//################################
// Message
//################################

typedef enum {
	CON = 0,    // Confirmable Message
	NON = 1,    // Non-confirmable Message
	ACK = 2,    // Acknowlegment Message
	RST = 3     // Reset Message
} CoAP_MessageType_t;

#define MESSAGE_CODE_CLASS_SHIFT            ( 5u )
#define CODE(CLASS, CODE)                   ( (CLASS << MESSAGE_CODE_CLASS_SHIFT) | CODE )
#define GET_CLASS_FROM_MESSAGE_CODE( CODE ) ( CODE >> MESSAGE_CODE_CLASS_SHIFT )

#define MESSAGE_CLASS_0 ( 0u )
#define MESSAGE_CLASS_2 ( 2u )
#define MESSAGE_CLASS_4 ( 4u )
#define MESSAGE_CLASS_5 ( 5u )

typedef enum {
	MSG_EMPTY = CODE(MESSAGE_CLASS_0, 0u),
	REQ_GET = CODE(MESSAGE_CLASS_0, 1u),
	REQ_POST = CODE(MESSAGE_CLASS_0, 2u),
	REQ_PUT = CODE(MESSAGE_CLASS_0, 3u),
	REQ_DELETE = CODE(MESSAGE_CLASS_0, 4u),
	REQ_FETCH = CODE(MESSAGE_CLASS_0, 5u),
	REQ_PATCH = CODE(MESSAGE_CLASS_0, 6u),
	REQ_IPATCH = CODE(MESSAGE_CLASS_0, 7u),
	REQ_LAST = CODE(MESSAGE_CLASS_0, 7u),
	RESP_FIRST_2_00 = CODE(MESSAGE_CLASS_2, 0u),
	RESP_SUCCESS_CREATED_2_01 = CODE(MESSAGE_CLASS_2, 1u),    // only used on response to "POST" and "PUT" like HTTP 201
	RESP_SUCCESS_DELETED_2_02 = CODE(MESSAGE_CLASS_2, 2u),    // only used on response to "DELETE" and "POST" like HTTP 204
	RESP_SUCCESS_VALID_2_03 = CODE(MESSAGE_CLASS_2, 3u),
	RESP_SUCCESS_CHANGED_2_04 = CODE(MESSAGE_CLASS_2, 4u),    // only used on response to "POST" and "PUT" like HTTP 204
	RESP_SUCCESS_CONTENT_2_05 = CODE(MESSAGE_CLASS_2, 5u),    // only used on response to "GET" like HTTP 200 (OK)
	RESP_SUCCESS_CONTINUE_2_31 = CODE(MESSAGE_CLASS_2, 31u),  // used for blockwise, see RFC 7959
	RESP_ERROR_BAD_REQUEST_4_00 = CODE(MESSAGE_CLASS_4, 0u),  // like HTTP 400
	RESP_ERROR_UNAUTHORIZED_4_01 = CODE(MESSAGE_CLASS_4, 1u),
	RESP_BAD_OPTION_4_02 = CODE(MESSAGE_CLASS_4, 2u),
	RESP_FORBIDDEN_4_03 = CODE(MESSAGE_CLASS_4, 3u),
	RESP_NOT_FOUND_4_04 = CODE(MESSAGE_CLASS_4, 4u),
	RESP_METHOD_NOT_ALLOWED_4_05 = CODE(MESSAGE_CLASS_4, 5u),
	RESP_METHOD_NOT_ACCEPTABLE_4_06 = CODE(MESSAGE_CLASS_4, 6u),
	RESP_REQUEST_ENTITY_INCOMPLETE_4_08 = CODE(MESSAGE_CLASS_4, 8u),
	RESP_PRECONDITION_FAILED_4_12 = CODE(MESSAGE_CLASS_4, 12u),
	RESP_REQUEST_ENTITY_TOO_LARGE_4_13 = CODE(MESSAGE_CLASS_4, 13u),
	RESP_UNSUPPORTED_CONTENT_FORMAT_4_15 = CODE(MESSAGE_CLASS_4, 15u),
	RESP_INTERNAL_SERVER_ERROR_5_00 = CODE(MESSAGE_CLASS_5, 0u),
	RESP_NOT_IMPLEMENTED_5_01 = CODE(MESSAGE_CLASS_5, 1u),
	RESP_BAD_GATEWAY_5_02 = CODE(MESSAGE_CLASS_5, 2u),
	RESP_SERVICE_UNAVAILABLE_5_03 = CODE(MESSAGE_CLASS_5, 3u),
	RESP_GATEWAY_TIMEOUT_5_04 = CODE(MESSAGE_CLASS_5, 4u),
	RESP_PROXYING_NOT_SUPPORTED_5_05 = CODE(MESSAGE_CLASS_5, 5u)
} CoAP_MessageCode_t;

// Granular control over response suppression (See section 2.1 of [RFC7967]).
#define NO_RESPONSE_FOR_CLASS_2 	( 0b00000010 ) // Not interested in 2.xx responses
#define NO_RESPONSE_FOR_CLASS_4 	( 0b00001000 ) // Not interested in 4.xx responses
#define NO_RESPONSE_FOR_CLASS_5 	( 0b00010000 ) // Not interested in 5.xx responses
#define NO_RESPONSE_SUPPRESS_ALL	( 0b00011010 ) // Not interested in all responses

typedef struct {
	uint8_t Length;
	uint8_t Token[8];
} CoAP_Token_t;

// Delcare CoAP_Res so it can be used in CoAP_Message_t
struct CoAP_Res;

typedef struct {
	uint32_t Timestamp; //set by parse/send network routines
	//VER is implicit = 1
	//TKL (Token Length) is calculated dynamically
	bool is_secured;                            // [1] Indication that msg is secured.
	CoAP_MessageType_t Type;                    // [1] T
	CoAP_MessageCode_t Code;                    // [1] Code
	uint16_t MessageID;                         // [2] Message ID (maps ACK msg to coresponding CON msg)
	uint16_t PayloadLength;                     // [2]
	uint16_t PayloadBufSize;                    // [2] size of allocated msg payload buffer
	CoAP_Token_t Token;                         // [9] Token (1 byte Length + up to 8 Byte for the token content)
	CoAP_option_t *pOptionsList;                // [4] linked list of Options
	uint8_t *Payload;                           // [4] MUST be last in struct! Because of mem allocation scheme which tries to allocate message mem and payload mem in ONE big data chunk

	struct CoAP_Res *pResource;                      // Pointer the the resource this message is intended for.
} CoAP_Message_t; //total of 25 Bytes

//################################
// Observer
//################################

typedef struct CoAP_Observer {
	NetEp_t Ep;                  // [16B]
	SocketHandle_t socketHandle; // [4B]
	uint8_t FailCount;           // [1B]
	CoAP_Token_t Token;          // [9B]
	CoAP_option_t *pOptList;     // [xxB](uri-host) <- will be removed if attached, uri-query, observe (for seq number)

	struct CoAP_Observer *next;  // [4B] pointer (linked list) (not saved while sleeping)
} CoAP_Observer_t;

//################################
// Resources
//################################

//Bitfields for resource BitOpts
#define RES_OPT_GET    (1 << REQ_GET)    // 1<<1
#define RES_OPT_POST   (1 << REQ_POST)   // 1<<2
#define RES_OPT_PUT    (1 << REQ_PUT)    // 1<<3
#define RES_OPT_DELETE (1 << REQ_DELETE) // 1<<4
#define RES_OPT_FETCH  (1 << REQ_FETCH)  // 1<<5
#define RES_OPT_PATCH  (1 << REQ_PATCH)  // 1<<6
#define RES_OPT_IPATCH (1 << REQ_IPATCH) // 1<<7

typedef enum {
	HANDLER_OK = 0,			// everything fine - we got an response to send
	HANDLER_POSTPONE = 1,	// Handler needs more time to fulfill request, send ACK and separate response
	HANDLER_ERROR = 2,		// handler has no result and will not deliver
	HANDLER_SKIPPED = 3,	// do not send confirm message if possible - NON request case
} CoAP_HandlerResult_t;

typedef CoAP_HandlerResult_t (*CoAP_ResourceHandler_fPtr_t)(CoAP_Message_t *pReq, CoAP_Message_t *pResp, void *clientCtx);
// TODO: Can we use the CoAP_ResourceHandler_fPtr_t signature also for notifiers?
typedef CoAP_HandlerResult_t (*CoAP_ResourceNotifier_fPtr_t)(CoAP_Observer_t *pObserver, CoAP_Message_t *pResp);

typedef void (*CoAP_ResourceObserverInfo_t)(CoAP_Observer_t *pObserver, bool active, struct CoAP_Res *pResource, void *clientCtx);

typedef enum {
    ENC_END_POINT_ENC   = 0,	// endpoint for encrypted messages only
    ENC_END_POINT_COAP  = 1,	// endpoint for unencrypted messages only
    ENC_END_POINT_MIXED = 2,	// endpoint for both encrypted and unencrypted messages
} CoAP_EncEndPoint_t;

typedef enum {
    COAP_RESP_NO_NON    = 0,	// no response for NON confirmable request
    COAP_RESP_NON_NON   = 1,	// NON confirmable response for NON confirmable request
} CoAP_ResponseType_t;


typedef struct {
	uint16_t Cf;    // Content-Format
	uint16_t AllowedMethods; // Bitwise resource options //todo: Send Response as CON or NON
	uint16_t ETag;
	CoAP_MessageType_t NotificationType;
	CoAP_EncEndPoint_t EncEndPoint;
	CoAP_ResponseType_t ResponseType;
} CoAP_ResOpts_t;

typedef struct CoAP_Res {
	struct CoAP_Res *next; //4 byte pointer (linked list)
	char *pDescription;
	uint32_t UpdateCnt; // Used as value for the Observe option
	CoAP_ResOpts_t Options;
	CoAP_option_t *pUri; //linked list of this resource URI options
	CoAP_Observer_t *pListObservers; //linked list of this resource observers
	CoAP_ResourceHandler_fPtr_t Handler;
	CoAP_ResourceNotifier_fPtr_t Notifier; //maybe "NULL" if resource not observable
	CoAP_ResourceObserverInfo_t ObserverInfo;
	void *context;
} CoAP_Res_t;

//################################
// Initialization
//################################


/*
 * API functions used by the CoAP stack. All fields need to be initialized.
 *
 * Fields marked as optional can be initialized with NULL
 */
typedef struct {
	// 1Hz Clock used by timeout logic
	uint32_t (*rtc1HzCnt)();
	// Uart/Display function to print debug/status messages
	void (*debugPuts)(const char *s);
	void (*debugArray)(const char *s, const uint8_t *array, size_t size);
	// Memory management:
	void *(*malloc)(size_t size);
	void (*free)(void *p);
	int (*rand)();
	// callback function for user that validates session
	bool (*is_session_valid)(void *session);
} CoAP_API_t;

//################################
// Public API (setup)
//################################

/**
 * Initialize the CoAP stack with a set of API functions used by the stack and a config struct.
 * @param api Struct with API functions that need to be defined for the stack to work
 * @param cfg Configuration values to setup the stack
 * @return A result code
 */
void CoAP_Init(CoAP_API_t api);

/**
 * Uninitialize the CoAP stack.
 * @param None.
 * @return COAP_OK            Coap uninitialized succesfully.
 * @return COAP_ERR_NOT_FOUND Failed to unlink resources.
 */
CoAP_Result_t CoAP_Uninit(void);

/**
 * Each CoAP implementation
 * @param handle
 * @return
 */
CoAP_Socket_t *CoAP_NewSocket(SocketHandle_t handle);

/**
 * Remove socket.
 * @param socket Network socket previously created using @ref CoAP_NewSocket.
 * @return COAP_OK            Socket has been succesfully removed.
 * @return COAP_ERR_ARGUMENT  Null pointer provided.
 * @return COAP_ERR_NOT_FOUND Socket not found.
 */
CoAP_Result_t CoAP_RemoveSocket(CoAP_Socket_t *socket);

/**
 * All resources must be created explicitly.
 * One reason is that the stack handles observer state per resource.
 * @param Uri
 * @param Descr
 * @param Options
 * @param pHandlerFkt
 * @param pNotifierFkt
 * @return
 */
CoAP_Res_t *CoAP_CreateResource(const char *Uri, const char *Descr, CoAP_ResOpts_t Options, CoAP_ResourceHandler_fPtr_t pHandlerFkt,
								CoAP_ResourceNotifier_fPtr_t pNotifierFkt, CoAP_ResourceObserverInfo_t pObserverInfo);

/**
 * Removes resource.
 * @param pResource Resource previously created using @ref CoAP_CreateResource.
 * @return COAP_OK            Resource has been succesfully removed.
 * @return COAP_ERR_ARGUMENT  Null pointer provided.
 * @return COAP_ERR_NOT_FOUND Resource not found.
 */
CoAP_Result_t CoAP_RemoveResource(CoAP_Res_t *pResource);

/**
 * Updates resource.
 * @param pResource Resource previously created using @ref CoAP_CreateResource.
 * @param options   Valid set of options.
 * @return COAP_OK            Resource has been succesfully updated.
 * @return COAP_ERR_ARGUMENT  Null pointer provided.
 * @return COAP_ERR_NOT_FOUND Resource not found.
 */
CoAP_Result_t CoAP_UpdateResource(CoAP_Res_t *pResource, CoAP_ResOpts_t options);

//#####################
// Message API
//#####################

// Set payload in resource handlers
// pMsgReq: The request message
// pMsgResp: The response message
// pPayload: The payload to be send
// payloadTotalSize: The size of pPayload
// payloadIsVolatile: Set to true for volatile memory.
//   If false, pPayload MUST point to static memory that is not freed before the interaction ends
//   which is hard to detect.
CoAP_Result_t
CoAP_SetPayload(CoAP_Message_t *pMsgResp, uint8_t *pPayload, size_t payloadTotalSize, bool payloadIsVolatile);

// Adds an option to the CoAP message
CoAP_Result_t CoAP_AddOption(CoAP_Message_t *pMsg, uint16_t OptNumber, uint8_t *buf, uint16_t length);

//########################################
// Interaction processing API
//########################################

// This function must be called by network drivers
// on reception of a new network packets which
// should be passed to the CoAP stack.
// "socketHandle" can be chosen arbitrary by calling network driver,
// but can be considered constant over runtime.
void CoAP_HandleIncomingPacket(SocketHandle_t socketHandle, NetPacket_t *pPacket);

/**
 * @brief Handle disconnect event from the transport layer.
 *		  When such event occurs, it means that transport has lost connection
 *		  with the device.
 *		  This function removes all observers and interactions that were
 *		  used by @ref transport_ctx.
 *
 * @param transport_ctx Transport context.
 * @retval COAP_OK			  Disconnect event handled successfully.
 * 		   COAP_ERR_NOT_FOUND Interaction correlated to the transport ctx
 * 							  has not been found. Such error can happen when
 * 							  observers were removed before disconnect event
 * 							  occured.
 */
CoAP_Result_t CoAP_handleDisconnectEvt(uint32_t transport_ctx);

/*
 * @brief Remove observer. Pending notifications will not be removed.
 * @param transport_ctx Transport context.
 * @retval COAP_OK
 * @retval COAP_ERR_NOT_FOUND Observer with provided transport does not exist.
 */

CoAP_Result_t CoAP_removeObserver(uint32_t transport_ctx);

/*
 * @brief Handle an error that happened in a lower layer - eg. transport layer. Send an error response withg provied CoAP code.
 * 
 * @param socketHandle Socket to send the response throught.
 * @param pPacket      Offending request.
 * @param responseCode CoAP Response code to send to the 
 */
void CoAP_HandleLowerLayerReceiverError(SocketHandle_t socketHandle, NetPacket_t* pPacket, CoAP_MessageCode_t responseCode);

/**
 * @brief Handle the response to require freshness from the client side by using ECHO option.
 * 		  (see RFC8613 appendix B.1.2).
 * 
 * @param socketHandle      Socket to send the response throught.
 * @param pPacket           Received request that is required to be resent as fresh.
 * @param echoValue         ECHO value generated by the server.
 * @param echoValueLength   ECHO value length in bytes.
 */
void CoAP_HandleResponseWithEcho(SocketHandle_t socketHandle, NetPacket_t* pPacket, uint8_t *echoValue, size_t echoValueLength);

// doWork must be called regularly to process pending interactions
void CoAP_doWork();

// drop all unfinished work
void CoAP_ClearPendingInteractions();

/*
 * @biref Calculate next interaction time in seconds.
 *        Can be used to put the device into sleep while waiting for the next interaction.
 * @retval Time of the next interaction or COAP_WAIT_FOREVER.
 */
uint32_t CoAP_getNextInteractionTime();

// Endpoint api
NetInterfaceType_t CoAP_ParseNetAddress(NetAddr_t *addr, const char *s);
#ifdef __cplusplus
}
#endif


#endif //LIBLOBARO_COAP_H
