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
*/
import "C"

import (
	"encoding/hex"
	"fmt"
	"unsafe"
)

//export notifyCallback
func notifyCallback(data unsafe.Pointer, len C.int) {
	// buffer := (*C.uchar)(data)
	fmt.Println(hex.EncodeToString(C.GoBytes(data, len)))
	fmt.Printf("Got notify message: len %d\n", len)
}

func main() {
	fmt.Println("-------------------------------")
	// C Library
	callbacks := C.Callbacks{}
	callbacks.notifyCallback = C.NotifyCallback(C.notify_callback)
	C.run_server(callbacks)
	fmt.Println("-------------------------------")
}
