package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"runtime"
)

func main() {
	flag.Parse()

	if flag.NArg() != 1 {
		Fatal("Usage: PORT")
	}
	addr, err := net.ResolveTCPAddr("tcp", flag.Arg(0))
	if err != nil {
		Fatal(err)
	}
	conn, err := net.DialTCP("tcp", nil, addr)
	if err != nil {
		Fatal(err)
	}
	defer conn.Close()

	w := bufio.NewWriterSize(conn, 8*1024)
	if _, err := io.Copy(w, os.Stdin); err != nil {
		w.Flush()
		Fatal(err)
	}
	if err := w.Flush(); err != nil {
		Fatal(err)
	}
}

func Fatal(err interface{}) {
	if err == nil {
		return
	}
	var s string
	if _, file, line, ok := runtime.Caller(1); ok && file != "" {
		s = fmt.Sprintf("Error (%s:%d)", filepath.Base(file), line)
	} else {
		s = "Error"
	}
	switch err.(type) {
	case error, string, fmt.Stringer:
		fmt.Fprintf(os.Stderr, "%s: %s\n", s, err)
	default:
		fmt.Fprintf(os.Stderr, "%s: %#v\n", s, err)
	}
	os.Exit(1)
}
