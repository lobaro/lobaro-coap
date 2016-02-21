//-----------------------------------------------------------------------
// For the moment, we just test if the library compiles fine for an 
// arduino environment.
//-----------------------------------------------------------------------
#include <coap.h>
#include "about_res.h"

// Generic Arduino implementations for coap_interface.h
extern "C" void hal_uart_puts(char *s) {
	Serial.println(s);
}
extern "C" void hal_uart_putc(char c) {
	Serial.print(c);
}

//1Hz Clock used by timeout logic
extern "C"  uint32_t hal_rtc_1Hz_Cnt(void) {
	return millis()/1000;
}

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
extern "C"  uint8_t* hal_nonVolatile_GetBufPtr() {
	return 0;
}

extern "C"  bool hal_nonVolatile_WriteBuf(uint8_t* data, uint32_t len) {
	return false;
}

void setup() {
	Serial.begin(115200);
	static uint8_t CoAP_WorkMemory[512]; //Working memory of CoAPs internal memory allocator
	CoAP_Init(CoAP_WorkMemory, sizeof(CoAP_WorkMemory));
	
	uint8_t ifID = 1;
	uint16_t LocalPort = 1234;
	
	NetSocket_t* pSocket;
	pSocket=RetrieveSocket2(ifID);
	if(pSocket != nullptr) {
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): interface ID already in use!\r\n");
		return;
	}
	
	pSocket = AllocSocket();
	if(pSocket == nullptr){
		ERROR("CoAP_ESP8266_CreateInterfaceSocket(): failed socket allocation\r\n");
		return;
	}
	
	//local side of socket
	pSocket->EpLocal.NetType = IPV4;
	pSocket->EpLocal.NetPort = LocalPort;
	
	//remote side of socket
	pSocket->EpRemote.NetType = IPV4;
	pSocket->EpRemote.NetPort = LocalPort;
	
	// No real implementation
	pSocket->Handle = nullptr;
	pSocket->RxCB = nullptr;
	pSocket->Tx  = nullptr;
	pSocket->Alive = true;
	
	pSocket->ifID = ifID;
	
	//example of large resource (blockwise transfers)
	Create_About_Resource();		
	
	coap_mem_determinateStaticMem();
	coap_mem_stats();
}

void loop() {
	CoAP_doWork();
}