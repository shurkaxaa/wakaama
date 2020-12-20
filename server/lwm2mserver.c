#include "lwm2m_api.h"
#include "log.h"

static void prv_notify_callback(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int count,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData)
{
    ZF_LOGE("Notify from client #%d ", clientID);
    // prv_printUri(uriP);
    ZF_LOGE(" number %d\n", count);
    // output_data(stdout, format, data, dataLength, 1);
    ZF_LOGE("\n> ");
}

int main(int argc, char *argv[])
{
    Callbacks cb;
    cb.notifyCallback = prv_notify_callback;
    run_server(cb);
    return 0;
}
