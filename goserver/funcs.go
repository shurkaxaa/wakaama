package main

/*
#include "lwm2m_api.h"
#include <stdlib.h>
#include <stdio.h>

extern void notifyCallback(void*, int);
extern int aaCallback(char*, uint8_t *, size_t, void *);
extern void readCallback(int status, uint8_t *data, int dataLength, void *user_data);

void notify_callback(char *endpoint, uint8_t *data, int dataLength)
{
    notifyCallback(data, dataLength);
};

int aa_callback(char *endpoint, uint8_t *authCode, size_t authCodeLen, void *userData)
{
	printf("AA callback %s\n", endpoint);
    return aaCallback(endpoint, authCode, authCodeLen, userData);
};

void read_callback(int status, uint8_t *data, int dataLength, void *user_data)
{
	printf("Read callback\n");
    readCallback(status, data, dataLength, user_data);
};
*/
import "C"
