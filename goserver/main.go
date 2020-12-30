package main

/*

#cgo CFLAGS: -I ../lib -I ../core
#cgo LDFLAGS: -L ../build -lwakaama -Wl,-rpath=../build

#include "lwm2m_api.h"

extern void notify_callback(char *endpoint, uint8_t *data, int dataLength);
extern void read_callback(int status, uint8_t *data, int dataLength, void *user_data);
extern int aa_callback(char *endpoint, uint8_t *authCode, size_t authCodeLen, void *userData);

*/
import "C"

import (
	"encoding/hex"
	"fmt"
	"time"
	"unsafe"

	gopointer "github.com/mattn/go-pointer"
)

//export notifyCallback
func notifyCallback(data unsafe.Pointer, len C.int) {
	fmt.Printf("Got notify message: len %d\n", len)
	fmt.Println(hex.EncodeToString(C.GoBytes(data, len)))
	// TODO call flow control, decode TLV, call GRPC forwarding API's, statistics, etc...
}

type readOp interface {
	complete()
}

//export readCallback
func readCallback(status C.int, readData unsafe.Pointer, len C.int, userData unsafe.Pointer) {
	fmt.Printf("Got read message: status: %d len %d\n", status, len)
	defer gopointer.Unref(userData)
	v := gopointer.Restore(userData).(readOp)
	v.complete()
}

type readOpImpl struct {
	id int
}

func (op *readOpImpl) complete() {
	fmt.Printf("Read completed: id %d\n", op.id)
}

func testRead(endpoint *C.char, obj, inst, res int) {
	op := &readOpImpl{id: -100}
	p := gopointer.Save(op)
	C.iotts_lwm2m_read(endpoint, 3, 0, 0, 10, p)
}

//export aaCallback
func aaCallback(endpoint *C.char, authCode unsafe.Pointer, authCodeLen C.int, data unsafe.Pointer) C.int {
	fmt.Printf("Device connection attempt %s %s\n", C.GoString(endpoint), string(C.GoBytes(authCode, authCodeLen)))
	fmt.Println(hex.EncodeToString(C.GoBytes(authCode, authCodeLen)))
	go func() {
		// just delay
		for {
			select {
			case <-time.After(time.Duration(3) * time.Second):
			}
			testRead(endpoint, 3, 0, 0)
		}
	}()
	return C.int(0)
	return C.int(1)
}

func main() {
	fmt.Println("-------------------------------")
	// C Library
	callbacks := C.Callbacks{}
	callbacks.notifyCallback = C.NotifyCallback(C.notify_callback)
	callbacks.aaCallback = C.AACallback(C.aa_callback)
	callbacks.readCallback = C.ReadCallback(C.read_callback)
	C.run_server(callbacks)
	fmt.Println("-------------------------------")
}
