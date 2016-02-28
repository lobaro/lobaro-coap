#include <iostream>
extern "C" {
#include <sys/timeb.h> 
}

time_t millis(void)
{
	struct timeb start;
	ftime(&start);
    return (start.time * 1000 + start.millitm);
}

// Generic implementations for coap_interface.h
extern "C" void hal_uart_puts(char *s) {
	std::cout << s << std::endl;
}
extern "C" void hal_uart_putc(char c) {
	std::cout << c;
}

//1Hz Clock used by timeout logic
extern "C"  uint32_t hal_rtc_1Hz_Cnt(void) {
	return (uint32_t) millis()/1000;
}

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
extern "C"  uint8_t* hal_nonVolatile_GetBufPtr() {
	return 0;
}

extern "C"  bool hal_nonVolatile_WriteBuf(uint8_t* data, uint32_t len) {
	return false;
}