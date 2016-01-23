/* 
 * File:   buffer.h
 * Author: Michael Winter <mail@michael-winter.me.uk>
 *
 * Created on 10 February 2015, 21:19
 */

#ifndef BUFFER_H
#define	BUFFER_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct buf buffer_t;
    typedef unsigned long ulong;

    /**
     * Add data to the buffer.
     * 
     * If *buffer is NULL, a new buffer is created.
     * 
     * @param buffer a pointer to a buffer pointer. This will be updated if the
     *               buffer is (re-)allocated.
     * @param data the data to store.
     * @param len the length of data to be added starting from *data.
     * @return true if the data was stored; false if buffer is NULL or there
     *         was insufficient memory.
     */
    int   buffer_append(buffer_t **const buffer, const char *const data, const ulong len);
    /**
     * Consume len bytes from the buffer, storing them in data.
     * 
     * @param buffer a pointer to the buffer from which data is to be
     *               extracted.
     * @param dest a pointer to where data is to be copied from the buffer.
     * @param len the number of bytes to read from the buffer.
     * @return the number of bytes read.
     */
    ulong buffer_take(buffer_t *const buffer, char *const dest, const ulong len);
    /**
     * Return the number of bytes remaining in the buffer.
     * 
     * @param buffer a pointer to the buffer to be queried.
     * @return the number of available bytes.
     */
    ulong buffer_remaining(const buffer_t *const buffer);
    /**
     * Free memory allocated to the buffer.
     * 
     * The pointer referenced by buffer will be set to NULL after this function
     * completes.
     * 
     * @param buffer a pointer-to-pointer of the buffer to be released.
     */
    void  buffer_release(buffer_t **const buffer);

#ifdef	__cplusplus
}
#endif

#endif	/* BUFFER_H */

