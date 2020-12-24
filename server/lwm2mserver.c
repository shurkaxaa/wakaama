#include "lwm2m_api.h"
#include "log.h"

static void prv_notify_callback(char *endpoint, uint8_t *data, int dataLength)
{
    ZF_LOGD("Notify from client #%s ", endpoint);
    // prv_printUri(uriP);
    // output_data(stdout, format, data, dataLength, 1);
    ZF_LOGD("\n> ");
}

int main(int argc, char *argv[])
{
    Callbacks cb;
    cb.notifyCallback = prv_notify_callback;
    run_server(cb);
    return 0;
}
