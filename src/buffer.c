#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"

#define INITIAL_BUFFER_SIZE 4096


/*
   ptr          head                  tail
    |            |XXXXXXXXXXXXXXXXXXXXXX|              |
    [ ---------------- size ---------------------------]
                                        [ spacecount   ]
                 [     datacount        ]

*/

int buffer_allocate(buffer_t *b, size_t size)
{
    b->size = size;
    b->ptr = (char *)malloc(b->size);
    if (!b->ptr) return -1;
// TODO (CEV): use calloc() is this is set
#ifdef SANITIZE_BUFFERS
    memset(b->ptr, 0, size);
#endif
    b->head = b->ptr;
    b->tail = b->ptr;
    return 0;
}

int buffer_init(buffer_t *b)
{
    return buffer_allocate(b, INITIAL_BUFFER_SIZE);
}

int buffer_init_contents(buffer_t *b, const char *data, size_t size)
{
    buffer_allocate(b, size);
    return buffer_set(b, data, size);
}

buffer_t *create_buffer(size_t size)
{
    buffer_t *ret = (buffer_t *)malloc(sizeof(*ret));
    if (0 != buffer_allocate(ret, size)) {
        free(ret);
        ret = NULL;
    }
    return ret;
}

size_t buffer_datacount(buffer_t *b)
{
    return b->tail - b->head;
}

size_t buffer_spacecount(buffer_t *b)
{
    return b->size - (b->tail - b->ptr);
}

char *buffer_head(buffer_t *b)
{
    return b->head;
}

char *buffer_tail(buffer_t *b)
{
    return b->tail;
}

int buffer_newsize(buffer_t *b, size_t newsize)
{
    char *pnew = realloc(b->ptr, newsize);
    if (!pnew)
        return -1;
    b->head = pnew + (b->head - b->ptr);
    b->tail = pnew + (b->tail - b->ptr);
    b->ptr = pnew;
    b->size = newsize;
    return 0;
}

int buffer_expand(buffer_t *b)
{
    return buffer_newsize(b, b->size * 2);
}

int buffer_consume(buffer_t *b, size_t amt)
{
    if (b->head + amt > b->tail)
        return -1;

    b->head += amt;
    return 0;
}

// TODO (CEV): replace with 'buffer_consume_to_eol' or something similar since
// that is the only way we use this function and using a constant token could
// help the compiler optimize this function.
int buffer_consume_until(buffer_t *b, char token)
{
    char *head = buffer_head(b);
    const size_t len = buffer_datacount(b);
    char *p = memchr(head, token, len);
    if (p != NULL) {
        buffer_consume(b, p - head + 1);
    } else {
        buffer_consume(b, len);
    }
    return 0;
}

int buffer_produced(buffer_t *b, size_t amt)
{
    if ((b->tail + amt) - b->ptr > b->size)
        return -1;

    b->tail += amt;
    return 0;
}

int buffer_set(buffer_t *b, const char *data, size_t size)
{
    if (b->size < size) {
        if (0 != buffer_newsize(b, size))
            return -1;
    }
    memcpy(b->ptr, data, size);
    return buffer_produced(b, size);
}

// CEV: this is only used for tests - delete
int buffer_append(buffer_t *b, const char *data, size_t size)
{
    if (size > buffer_spacecount(b))
        return -1;

    memcpy(b->tail, data, size);
    return buffer_produced(b, size);
}

static inline void buffer_realign_impl(buffer_t *b)
{
    // CEV: only slide the buffer down if the head
    // has been advanced
    if (b->head != b->ptr) {
        memmove(b->ptr, b->head, b->tail - b->head);
        /* do not switch the order of the following two statements */
        b->tail = b->ptr + (b->tail - b->head);
        b->head = b->ptr;
    }
}

int buffer_realign(buffer_t *b)
{
    buffer_realign_impl(b);
    return 0;
}

int buffer_grow(buffer_t *b, size_t size) {
    if (buffer_spacecount(b) < size) {
        buffer_realign_impl(b);
        // CEV: re-size if we could not free enough space
        if (buffer_spacecount(b) < size) {
            if (buffer_newsize(b, (b->size * 2) + size) != 0) {
                 return -1;
            }
        }
    }
    return 0;
}

// TODO (CEV): replace buffer_append with this
int buffer_write(buffer_t *b, const char *restrict s, size_t len)
{
    if (buffer_grow(b, len) == 0) {
        memcpy(buffer_tail(b), s, len);
        return buffer_produced(b, len);
    }
    return -1;
}

void buffer_destroy(buffer_t *b)
{
    if (b->ptr)
        free(b->ptr);
    b->ptr = NULL;
    b->head = NULL;
    b->tail = NULL;
    b->size = 0;
}

void delete_buffer(buffer_t *b)
{
    buffer_destroy(b);
    free(b);
}

void buffer_wrap(buffer_t *b, const char *data, size_t size)
{
    // WARN (CEV): make sure we never modify this !!!

    /* Promise not to modify */
    b->ptr = (char *)data;
    b->head = b->ptr;
    b->tail = b->head + size;
    b->size = 0;
}

/*
// WARN (CEV): we should use this for stats
int buffer_sprintf(buffer_t *b, const char *fmt, ...)
{
    int size;
    va_list args1;
    va_list args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    size = vsnprintf(NULL, 0, fmt, args1);
    if (size < 0) {
        goto cleanup;
    }
    size_t cap = buffer_spacecount(b);
    if (cap <= size) {
        if (buffer_grow(b, size + 1) != 0) {
            size = -1;
            goto cleanup;
        }
        cap = buffer_spacecount(b);
    }
    size = vsnprintf(buffer_head(b), cap, fmt, args2);
    if (size < 0) {
        goto cleanup; // error
    }
    if (size >= cap) {
        goto cleanup; // WARN (CEV): failed to allocate enough space
    }
    buffer_produced(b, size);

cleanup:
    va_end(args1);
    va_end(args2);
    return size;
}
*/
