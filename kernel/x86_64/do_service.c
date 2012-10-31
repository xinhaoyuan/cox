#include <arch/do_service.h>

void
service_context_transfer(service_context_t dst, service_context_t src)
{
    SERVICE_ARG0_SET(dst, SERVICE_ARG0_GET(src));
    SERVICE_ARG1_SET(dst, SERVICE_ARG1_GET(src));
    SERVICE_ARG2_SET(dst, SERVICE_ARG2_GET(src));
    SERVICE_ARG3_SET(dst, SERVICE_ARG3_GET(src));
    SERVICE_ARG4_SET(dst, SERVICE_ARG4_GET(src));
    SERVICE_ARG5_SET(dst, SERVICE_ARG5_GET(src));
}
