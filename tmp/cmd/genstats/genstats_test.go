package main

import (
	"bufio"
	"math/rand"
	"net"
	"os"
	"testing"
	"time"
)

func init() {
	initRandOnce.Do(func() { rand.Seed(time.Now().UnixNano()) })
}

func BenchmarkGauge(b *testing.B) {
	address := os.Getenv("GS_ADDR")
	if address == "" {
		address = ":3004"
	}
	addr, err := net.ResolveTCPAddr("tcp", address)
	if err != nil {
		Fatal(err)
	}
	conn, err := net.DialTCP("tcp", nil, addr)
	if err != nil {
		Fatal(err)
	}
	defer conn.Close()

	keys := CreateKeys(100)

	w := bufio.NewWriterSize(conn, 8*1024)
	buf := make([]byte, 256)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		key := keys[i%len(keys)]
		_, err := w.Write(appendGauge(buf[:0], key, rand.Uint64()))
		if err != nil {
			w.Flush()
			Fatal(err)
		}
	}
}
