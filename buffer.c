#include "buffer.h"

/* Normally, the buffer is reallocated once half its current capacity has been
 * consumed. To minimise the number of reallocations, at least this number of
 * bytes must be unused.
 */
#define MIN_WASTEAGE 4096
/* To avoid excessive waste, once this limit is exceeded the buffer will be
 * reallocated during the next buffer_append call regardless of the amount of
 * data being added.
 */
#define MAX_WASTEAGE 16777216

#if MIN_WASTEAGE > MAX_WASTEAGE
#   error MIN_WASTEAGE must be less than MAX_WASTEAGE
#endif

struct buf {
    char *data;
    ulong size;
    ulong offset;
};


int buffer_append(buffer_t **const buffer, const char *const data, const ulong len) {
    if (!buffer)
        return 0;
    if (!*buffer) {
        *buffer = calloc(1, sizeof(struct buf));
        if (!*buffer)
            return 0;
    }
    
    if (len) {
        char *dest;
        if (((*buffer)->offset < MIN_WASTEAGE) || (((*buffer)->offset < MAX_WASTEAGE)
                && ((*buffer)->offset > ((*buffer)->size >> 2)))) {
            dest = realloc((*buffer)->data, (*buffer)->size + len);
        } else {
            const ulong remaining = (*buffer)->size - (*buffer)->offset;
            dest = malloc(remaining + len);
            if (dest) {
                memcpy(dest, (*buffer)->data + (*buffer)->offset, remaining);
                (*buffer)->size = remaining;
                (*buffer)->offset = 0;
                free((*buffer)->data);
            }
        }
        if (!dest)
            return 0;
        (*buffer)->data = dest;
        memcpy((*buffer)->data + (*buffer)->size, data, len);
        (*buffer)->size += len;
    }
    return 1;
}


ulong buffer_take(buffer_t *const buffer, char *const dest, const ulong len) {
    ulong remaining;
    ulong taken;
    if (!buffer || !buffer->data)
        return 0;

    remaining = buffer->size - buffer->offset;
    taken = (remaining < len) ? remaining : len;
    memcpy(dest, buffer->data + buffer->offset, taken);
    buffer->offset += taken;
    
    if (buffer->offset == buffer->size) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->offset = 0;
        buffer->size = 0;
    }
    return taken;
}


ulong buffer_remaining(const buffer_t *const buffer) {
    if (!buffer)
        return 0;
    return buffer->size - buffer->offset;
}


void buffer_release(buffer_t **const buffer) {
    if (!buffer || !*buffer)
        return;

    free((*buffer)->data);
    free(*buffer);
    *buffer = NULL;
}
