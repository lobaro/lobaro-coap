
# Porting guide

This library is independent of the underlying framework it works on, so you can easily use it on whatever platform. This guide explains how to use the library on your specific platform.

In summary, you need to implement the following items:

1. Configure a few standard functions the library needs to call (ex: for memory management, printing, etc.) using the `CoAP_Init` function to set them up.
2. Implement a function to create a socket (network interface) that will be used by the library.
3. Implement a transmission function that will be called by the library to send a packet on the given interface.
4. Have your application periodically call the `CoAP_doWork()` function to process the CoAP interactions that are pending. This can be done in a separate thread/process.
5. Have your application periodically read the packets from the network interface and transfer them to the library using `CoAP_HandleIncomingPacket` function.

## Example

To demonstrate those items, we'll use a simple example of an embedded system running FreeRTOS, with a POSIX socket network interface and configured to run as a CoAP client.
The FreeRTOS allows us to create a separate task to handle the periodical work (item 4 and 5). The Posix socket API is how we access the network interface, we will have lobaro-coap indirectly talk to this API.

Lets start by the `main` in which we configure the standard functions to use (*item 1*). For memory allocation, we use the FreeRTO version of `malloc` (`pvPortMalloc`) and `free` (`vPortFree`). For the rest we wrote small custom functions.

```cpp
#include <FreeRTOS.h>
#include <socket.h>
#include "lobaro-coap/src/coap.h"

void debugPuts(char* s)
{
	printf("%s", s);
}

uint32_t elapsedTime_seconds()
{
	return xTaskGetTickCount() / configTICK_RATE_HZ; // Returns the elapsed time since reset, in seconds
}

int generateRandom()
{
	// Use whatever random number generator you have access to to generate the number here
	// int randomNumber = ...
	return randomNumber;
}

void main()
{
	// Bind system functions to the CoAP library
	CoAP_API_t api = {
		.malloc = pvPortMalloc,				// Function for allocating memory
		.free = vPortFree,					// Function for freeing memory
		.rtc1HzCnt = elapsedTime_seconds,	// Function that returns a time in seconds
		.rand = generateRandom,				// Function to generate random numbers
		.debugPuts = debugPuts,				// Function to print info for debugging
	};

	CoAP_Init(api);

	// Create the task that will process the CoAP work
	if (pdPASS != xTaskCreate(coapClient_work_task, "CoapWork", COAP_CLIENT_TASK_STACK_SIZE, NULL, COAP_CLIENT_TASK_PRIORITY, NULL)) {
		// Handle error
	}

	vTaskStartScheduler(); // Run the task

	return 0;
}

```

In the main we also create a new task that will do the CoAP work. The function that this task will execute is implemented below.
It runs continuously as a separate thread. It has two roles: 1) transfers received packets to the library and 2) calls `CoAP_doWork()`, both done periodically.

```cpp
static void coapClient_work_task()
{
	// Create the buffer we need to read packets from the network
	#define RX_BUFFER_SIZE (MAX_PAYLOAD_SIZE + 127)
	static uint8_t rxBuffer[RX_BUFFER_SIZE];
	static SocketHandle_t sockHandle = 0;

	// Create the socket (network interface)
	if(!CoAP_Posix_CreateSocket(&sockHandle, IPV4)) {
		// Handle the error
	}

	while(true)	{
		
		// Firts, read all the pending packets from the network
		// interface and transfer them to the coap library
		int res = 0;
		do {
			// Read from network interface (using Posix socket api)
			res = recv(sockHandle, rxBuffer, RX_BUFFER_SIZE, MSG_DONTWAIT);

			if(res > 0)
			{
				printf("New packet received on interface, bytes read = %d", res);
			
				// Format the packet to the proper structure
				NetPacket_t pckt;
				memset(&pckt, 0, sizeof(pckt));
				pckt.pData = rxBuffer;
				pckt.size = res;
				pckt.remoteEp = serverEp;

				CoAP_HandleIncomingPacket(sockHandle, &pckt); 	// Feed the received packet to the CoAP library
																// Note: this will copy the data to a new
																// buffer (we can reuse the rxBuffer)
			}

		} while(res > 0);

		// Then process any pending work
		CoAP_doWork();

		// Then sleep for 100 ms
		vTaskDelay(100);
	}
}
```

At the begining of the task, we create a new socket that the library will use. The function for creating that socket is shown below (*item 2*).

```cpp
// Function to create a "CoAP" socket that can be used with the CoAP library
// Returns true and sets the `handle` on success
// Returns false if the socket could not be created
bool CoAP_Posix_CreateSocket(SocketHandle_t *handle, NetInterfaceType_t type)
{
	if(type == IPV4) {

		// Create the actual Posix socket
		int posixSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if(posixSocket < 0) {
			ERROR("Could not create socket, errno = %d", errno);
			return false;
		}

		// Allocate a new CoAP_Socket_t space for this socket
		CoAP_Socket_t *newSocket = AllocSocket();

		if(newSocket == NULL) {
			ERROR("Could not allocate memory for new socket");
			close(socket);
			return false;
		}

		newSocket->Handle = posixSocket;
		newSocket->Tx = CoAP_Posix_SendDatagram; // Function to transmit packets
		newSocket->Alive = true; // UDP sockets don't need to be connected
		*handle = posixSocket;
		return true;
	}
	else {
		ERROR("Unsupported net type %d", type);
	}
	
	return false;
}
```

When creating the socket, we need to configure which function to use to transmit a packet (*item 3*) on this interface. We called it `CoAP_Posix_SendDatagram`.The function basically needs to send the given packet (`pckt`) to the specified interface (`socketHandle`) and return true on success or false on failure.

```cpp
// Function to send a packet to the network interface
bool CoAP_Posix_SendDatagram(SocketHandle_t socketHandle, NetPacket_t *pckt)
{
	int sock = (int)socketHandle;

	// Format the endpoint info from the pckt to the right structure
	// that we need in our specific network (Posix socket api)
	struct sockaddr addr;
	size_t sockaddrSize;
	if(pckt->remoteEp.NetType == IPV4)
	{
		struct sockaddr_in *remote = &addr;
		remote->sin_family = AF_INET;
		remote->sin_port = htons(pckt->remoteEp.NetPort);
		for(uint8_t i = 0; i < 4; i++)
			remote->sin_addr.s4_addr[i] = pckt->remoteEp.NetAddr.IPv4.u8[i];
		sockaddrSize = sizeof(struct sockaddr_in);
	}
	else
	{
		ERROR("Unsupported NetType : %d\n", pckt->remoteEp.NetType);
		return false;
	}

	// Actually send the packet to the network (Posix socket api)
	int ret = sendto(sock, pckt->pData, pckt->size, 0, &addr, sockaddrSize);

	if(ret < 0)
		ERROR("sendto() returned %d (errno = %d)\n", ret, errno);

	return ret > 0;
}
```

This transmission function will be called by the lobaro-coap library to send CoAP packets on the network.

To send a message from your application, you can use the `CoAP_StartNewRequest` function provided by lobaro-coap.

```cpp
void sendCoapMessage()
{
	// Send a CoAP message
	CoAP_Result_t result = CoAP_StartNewRequest(
		REQ_POST,				// (CoAP_MessageCode_t)
		"/path/to/ressource",	// (char*)
		sockHandle,				// (SocketHandle_t)
		&serverEndpoint,		// (NetEp_t*)
		CoAP_RespHandler_fn,	// The function that will be called
								// when the message gets a response
								// or fails to be sent
		data,					// Message data buffer (uint8_t *)
		length					// Message data length (size_t)
	);
}
```

As shown above, when using `CoAP_StartNewRequest` you may define a response handler function that will be called when the message gets a reply (for confirmable messages) or if it fails to get acknowledge after a few retries.

```cpp
// Response handler function
CoAP_Result_t CoAP_RespHandler_fn(CoAP_Message_t* pRespMsg, NetEp_t* sender)
{
	if(pRespMsg == NULL) {
		printf("CoAP message transmission failed after all retries (timeout)");
		return COAP_OK;
	}

	printf("Got a reply for MiD: %d", pRespMsg->MessageID);
	CoAP_PrintMsg(pRespMsg);

	return COAP_OK;
}
```  

The packet flow withing the library looks like this:

1. You send a message by calling `CoAP_StartNewRequest`
2. The lobaro-coap library will send that message for you calling the `CoAP_Posix_SendDatagram` function.
3. Eventually the server will reply and the response will be picked up by the `recv` function called in the `coapClient_work_task`. The reply will be transfered to the lobaro-coap library with our call to `CoAP_HandleIncomingPacket`
4. The lobaro-coap will call our `CoAP_RespHandler_fn` function upon the receiption of the reply
