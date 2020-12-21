package main

/*
#include "lwm2m_api.h"
#include <stdlib.h>
#include <stdio.h>

extern void notifyCallback(void*, int);
extern int aaCallback(char*, uint8_t *, size_t, void *);

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

int aa_callback(char *endpoint, uint8_t *authCode, size_t authCodeLen, void *userData)
{
	printf("AA callback %s\n", endpoint);
    return aaCallback(endpoint, authCode, authCodeLen, userData);
};
*/
import "C"
