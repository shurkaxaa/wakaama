#ifndef _LWM2M_API_H_
#define _LWM2M_API_H_

#include "liblwm2m.h"

typedef void (*NotifyCallback)(
    char *endpoint,
    // lwm2m_media_type_t format,
    uint8_t *data,
    int dataLength);

typedef int (*AACallback)(
    char *endpoint,
    uint8_t *authCode,
    size_t authCodeLen,
    void *userData);

typedef int (*ConnectedCallback)(char *endpoint);
typedef int (*DisconnectedCallback)(char *endpoint);
typedef void (*ReadCallback)(int status, uint8_t *data, int dataLength, void *user_data);

typedef struct
{
  NotifyCallback notifyCallback;
  AACallback aaCallback;
  ConnectedCallback connectedCallback;
  DisconnectedCallback disconnectedCallback;
  ReadCallback readCallback;
  /*
  1. dynamic write/execute/observe/notify/... callbacks for downstream?
  */
} Callbacks;

int run_server(Callbacks cb);

int iotts_lwm2m_read(char *endpoint,
                      int32_t objId,
                      int32_t intId,
                      int32_t resId,
                      int timeout,
                      void *user_data);

#endif
