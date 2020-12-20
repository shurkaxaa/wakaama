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

typedef struct {
  NotifyCallback notifyCallback;
} Callbacks;

int run_server(Callbacks cb);
#endif
