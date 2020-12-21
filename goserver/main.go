package main

/*

#cgo CFLAGS: -I ../lib -I ../core
#cgo LDFLAGS: -L ../build -lwakaama -Wl,-rpath=../build

#include "lwm2m_api.h"

extern void notify_callback(uint16_t clientID,
                            lwm2m_uri_t *uriP,
                            int count,
                            lwm2m_media_type_t format,
                            uint8_t *data,
                            int dataLength,
                            void *userData);

extern int aa_callback(char *endpoint, uint8_t *authCode, size_t authCodeLen, void *userData);

*/
import "C"

import (
	"encoding/hex"
	"fmt"
	"unsafe"
)

//export notifyCallback
func notifyCallback(data unsafe.Pointer, len C.int) {
	fmt.Printf("Got notify message: len %d\n", len)
	fmt.Println(hex.EncodeToString(C.GoBytes(data, len)))
	// TODO call flow control, decode TLV, call GRPC forwarding API's, statistics, etc...
}

//export aaCallback
func aaCallback(endpoint *C.char, authCode unsafe.Pointer, authCodeLen C.int, data unsafe.Pointer) C.int {
	fmt.Printf("Device connection attempt %s %s\n", C.GoString(endpoint), string(C.GoBytes(authCode, authCodeLen)))
	fmt.Println(hex.EncodeToString(C.GoBytes(authCode, authCodeLen)))
	// TODO call GRPC AA, statistics (connection success failure)?
	return C.int(0)
	return C.int(1)
}

func main() {
	fmt.Println("-------------------------------")
	// C Library
	callbacks := C.Callbacks{}
	callbacks.notifyCallback = C.NotifyCallback(C.notify_callback)
	callbacks.aaCallback = C.AACallback(C.aa_callback)
	C.run_server(callbacks)
	fmt.Println("-------------------------------")
}
