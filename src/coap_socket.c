//
// Created by Tobias on 27.02.2018.
//

#include <stdbool.h>
#include "coap.h"
#include "coap_socket.h"
#include "coap_message.h"

CoAP_Result_t _rom
CoAP_SendMsg(CoAP_Message_t* Msg, SocketHandle_t socketHandle, NetEp_t receiver) {
	INFO("Sending CoAP msg\r\n");

	uint16_t bytesToSend = 0;
	CoAP_Socket_t* pSocket = RetrieveSocket(socketHandle);

	if (pSocket == NULL) {
		ERROR("Socket not found! handle: %d\r\n", (int) socketHandle);
		return
				COAP_NOT_FOUND;
	}

	NetTransmit_fn SendPacket = pSocket->Tx;
	uint8_t quickBuf[16]; //speed up sending of tiny messages

	if (SendPacket == NULL) {
		ERROR("SendPacket function not found! handle: %d\r\n", socketHandle);
		return
				COAP_NOT_FOUND;
	}

// build generic packet
	NetPacket_t pked;
	pked.remoteEp = receiver;

//Alloc raw memory
	pked.size = CoAP_GetRawSizeOfMessage(Msg);
	if (pked.size <= 16) { //for small messages don't take overhead of mem allocation
		pked.pData = quickBuf;
	} else {
		pked.pData = (uint8_t*) CoAP.api.malloc(pked.size);
		if (pked.pData == NULL)
			return COAP_ERR_OUT_OF_MEMORY;
	}

// serialize msg
	CoAP_BuildDatagram((pked.pData), &bytesToSend, Msg);

	if (bytesToSend != pked.size) {
		INFO("(!!!) Bytes to Send = %d estimated = %d\r\n", bytesToSend,
			 CoAP_GetRawSizeOfMessage(Msg)
		);
	}

	INFO("\r\no>>>>>>>>>>>>>>>>>>>>>>\r\nSend Message [%d Bytes], Interface #%u\r\n", bytesToSend, socketHandle);
	INFO("Receiving Endpoint: ");
	PrintEndpoint(&(pked.remoteEp));
	INFO("\n");

	int i;
	for (i = 0; i < pked.size; i++) {
		if (pked.pData[i] != 0) { //0 = string end
			INFO("0x%02x(%c) ", pked.pData[i], pked.pData[i]);
		} else {
			INFO("0x00() ");
		}
	}
	INFO("\r\n");

	if (SendPacket(socketHandle, &pked) == true) { // send COAP_OK!
		Msg->Timestamp = CoAP.api.rtc1HzCnt();
		CoAP_PrintMsg(Msg);
		INFO("o>>>>>>>>>>OK>>>>>>>>>>\r\n");
		if (pked.pData != quickBuf) {
			CoAP.api.free(pked.pData);
		}
		return COAP_OK;
	} else {
		CoAP_PrintMsg(Msg);
		INFO("o>>>>>>>>>>FAIL>>>>>>>>>>\r\n");
		if (pked.pData != quickBuf) {
			CoAP.api.free(pked.pData);
		}
		return COAP_ERR_NETWORK;
	}
}

//send minimal 4Byte header CoAP empty ACK message
CoAP_Result_t _rom CoAP_SendEmptyAck(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)

	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = ACK;
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

//send minimal 4Byte header CoAP empty RST message
CoAP_Result_t _rom CoAP_SendEmptyRST(uint16_t MessageID, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.MessageID = MessageID;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}

//send short response
CoAP_Result_t _rom CoAP_SendShortResp(CoAP_MessageType_t Type, CoAP_MessageCode_t Code, uint16_t MessageID, CoAP_Token_t token, SocketHandle_t socketHandle, NetEp_t receiver) {
	CoAP_Message_t Msg; //put on stack (no need to free)
	CoAP_InitToEmptyResetMsg(&Msg);
	Msg.Type = Type;
	Msg.MessageID = MessageID;
	Msg.Code = Code;
	Msg.Token = token;
	return CoAP_SendMsg(&Msg, socketHandle, receiver);
}