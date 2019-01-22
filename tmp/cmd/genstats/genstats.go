package main

import (
	"bufio"
	"bytes"
	"compress/gzip"
	"flag"
	"fmt"
	"io/ioutil"
	"math/rand"
	"net"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

var initRandOnce sync.Once

/*
func (s *tcpStatsdSink) FlushCounter(name string, value uint64) {
	s.flushString("%s:%d|c\n", name, value)
}

func (s *tcpStatsdSink) FlushGauge(name string, value uint64) {
	s.flushString("%s:%d|g\n", name, value)
}

func (s *tcpStatsdSink) FlushTimer(name string, value float64) {
	s.flushString("%s:%f|ms\n", name, value)
}
*/

var wordDict []string
var wordDictOnce sync.Once

func LoadWordDict() []string {
	wordDictOnce.Do(func() {
		f, err := os.Open("words.gz")
		if err != nil {
			panic(err)
		}
		defer f.Close()
		r, err := gzip.NewReader(f)
		if err != nil {
			panic(err)
		}
		b, err := ioutil.ReadAll(r)
		if err != nil {
			panic(err)
		}
		lines := bytes.Split(b, []byte{'\n'})
		wordDict = make([]string, 0, len(lines))
		for _, s := range lines {
			if s = bytes.TrimSpace(s); len(s) != 0 {
				wordDict = append(wordDict, string(s))
			}
		}
		sort.Strings(wordDict)
	})
	a := make([]string, len(wordDict))
	copy(a, wordDict)
	return a
}

var seenKeys = make(map[string]bool)

func genKey() string {
	initRandOnce.Do(func() { rand.Seed(time.Now().UnixNano()) })

	words := LoadWordDict()
	s := make([]string, rand.Intn(5)+1)
	for i := range s {
		x := words[rand.Intn(len(words)-1)]
		if x != "" {
			s[i] = x
		}
	}
	key := strings.Join(s, "_")
	if seenKeys[key] {
		return genKey()
	}
	return key
}

func CreateKeys(n int) []string {
	a := make([]string, n)
	for i := range a {
		a[i] = genKey()
	}
	return a
}

func appendGauge(buf []byte, name string, val uint64) []byte {
	buf = append(buf, name...)
	buf = append(buf, ':')
	buf = strconv.AppendUint(buf, val, 10)
	return append(buf, "|g\n"...)
}

func WriteStdout() {
	const N = 100
	keys := CreateKeys(100)
	w := bufio.NewWriterSize(os.Stdout, 8*1024)
	buf := make([]byte, 128)
	for i := uint64(0); ; i++ {
		key := keys[i%N]
		buf = append(buf[:0], key...)
		buf = append(buf, ':')
		buf = strconv.AppendUint(buf, i, 10)
		buf = append(buf, "|g\n"...)
		_, err := w.Write(buf)
		if err != nil {
			w.Flush()
			Fatal(err)
		}
	}
}

func WriteStats(address string, stop chan struct{}) {
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
	start := time.Now()
	var count uint64
	var size uint64
	for i := 0; ; i++ {
		select {
		case <-stop:
			d := time.Since(start)
			const MB = 1024 * 1024
			fmt.Printf("time:  %s  count: %d  size: %d/b %.2f/mb\n", d, count, size, float64(size)/MB)
			fmt.Printf("count: %.2f sec  %d ns/op\n", float64(count)/d.Seconds(), uint64(d)/count)
			fmt.Printf("size:  %.2f MB/s\n", (float64(size)/MB)/d.Seconds())
			return
		default:
			key := keys[i%len(keys)]
			n, err := w.Write(appendGauge(buf[:0], key, rand.Uint64()))
			// _, err := fmt.Fprintf(w, "%s:%d|g\n", key, rand.Uint64())
			if err != nil {
				w.Flush()
				Fatal(err)
			}
			count++
			size += uint64(n)
		}
	}
}

func main() {
	stdout := flag.Bool("stdout", false, "Write to stdout out instead of PORT")
	flag.Parse()

	if *stdout {
		WriteStdout()
		return
	}

	if flag.NArg() != 1 {
		Fatal("Usage: PORT")
	}

	done := make(chan struct{})
	wg := new(sync.WaitGroup)
	wg.Add(1)
	go func() {
		defer wg.Done()
		WriteStats(flag.Arg(0), done)
	}()

	sigs := make(chan os.Signal, 8)
	signal.Notify(sigs, os.Interrupt)
	<-sigs
	fmt.Fprintln(os.Stderr, "stopping...")
	close(done)
	wg.Wait()
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
