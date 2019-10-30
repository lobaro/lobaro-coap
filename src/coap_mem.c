#include "coap.h"
#include "coap_mem.h"

void CoAP_free(void *a) {
	CoAP.api.free(a);
}

void *CoAP_malloc(size_t size) {
	return CoAP.api.malloc(size);
}

void *CoAP_malloc0(size_t size) {
	void *a = CoAP.api.malloc(size);
	if (a != NULL) {
		memset(a, 0, size);
	}
	return a;
}
