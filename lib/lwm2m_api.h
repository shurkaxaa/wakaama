#ifndef _LWM2M_API_H_
#define _LWM2M_API_H_

#include "liblwm2m.h"

typedef void (*NotifyCallback)(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int count,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData);

typedef int (*AACallback)(
  char *endpoint,
  uint8_t *authCode,
  size_t authCodeLen,
  void *userData);

typedef struct {
  NotifyCallback notifyCallback;
  AACallback aaCallback;
  /*
  1. register/deregister callback?
  2. dynamic read/write/execute/observe/notify/... callbacks for downstream?
  */
} Callbacks;

int run_server(Callbacks cb);
#endif
