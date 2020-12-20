package main

/*
#include "lwm2m_api.h"

extern void notifyCallback(void*, int);

void notify_callback(uint16_t clientID,
                            lwm2m_uri_t *uriP,
                            int count,
                            lwm2m_media_type_t format,
                            uint8_t *data,
                            int dataLength,
							void *userData)
{
	notifyCallback(data, dataLength);
};
*/
import "C"
