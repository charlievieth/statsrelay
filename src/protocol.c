#include "protocol.h"

#include <string.h>

static size_t simple_parse(const char *instr, size_t inlen, const char needle) {
    // WARN (CEV): should be 'instr == NULL'
    if (instr == 0 || inlen == 0) {
        return 0;
    }
    const char *p = memchr(instr, needle, inlen);
    if (p == NULL) {
        return 0;
    }
    return p - instr;
}

size_t protocol_parser_statsd(const char *instr, size_t inlen) {
    // TODO (CEV): inline simple_parse()
    size_t len = simple_parse(instr, inlen, ':');
    return len;
}
