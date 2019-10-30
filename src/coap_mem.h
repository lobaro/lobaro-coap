#ifndef SRC_COAP_COAP_MEM_H_
#define SRC_COAP_COAP_MEM_H_

#include <stddef.h>

void *CoAP_malloc(size_t size);
void *CoAP_malloc0(size_t size);
void CoAP_free(void *a);

#endif
