/* 
 * File:   lru_cache.h
 * Author: Michael Winter <mail@michael-winter.me.uk>
 *
 * Created on 19 February 2015, 10:29
 */

#ifndef LRU_CACHE_H
#define	LRU_CACHE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

    typedef unsigned long ulong;
    
    /**
     * A cache with a least recently used retention policy.
     * 
     * There are two definitions of ordering for entries in the cache: by
     * identifier and by last use. The lru_cache_first and lru_cache_last
     * functions operate under the former, where the entry with the lowest
     * identifier value at any given time is considered "first". The retention
     * policy operates under the latter definition.
     * 
     * When an entry is added or retrieved from the cache, it is moved to the
     * head of an internal usage list. Once the cache is full, the entry in the
     * tail position is replaced.
     */
    typedef struct lru_cache lru_cache_t;
    
    /**
     * Entries retrieved from the cache are presented in an instance of this
     * structure.
     * 
     * The values herein should not be modified directly. Data can be changed
     * by calling lru_cache_put with the same identifier. Entries are replaced
     * automatically in accordance with the retention policy. Cache behaviour
     * is undefined if any of these fields are altered.
     */
    typedef struct {
        /**
         * The unique identifier for this cache entry.
         */
        ulong id;
        /**
         * A pointer to the cached data.
         */
        char *data;
        /**
         * The length of the data in this entry.
         */
        ulong size;
    } lru_cache_entry_t;
    
    /**
     * Allocate and initialise the least recently used cache.
     * 
     * @param blocks the maximum number of entries that can comprise this
     *               cache. Cannot be zero (0).
     * @param block_size the maximum size of each entry in the cache. Cannot be
     *                   zero (0).
     * @return the cache on success; NULL if insufficient memory.
     */
    lru_cache_t *lru_cache_init(const ulong blocks, const ulong block_size);
    /**
     * Deallocate the cache and the data stored therein.
     * 
     * @param cache a pointer to the cache to deallocate.
     */
    void lru_cache_free(lru_cache_t *const cache);
    
    /**
     * Get an entry from the cache with the corresponding identifier.
     * 
     * Successful retrieval promotes the entry to the head of the usage list.
     * 
     * This function runs in O(log n) time, where n is the number of entries.
     * 
     * @param cache a pointer to the cache to search for an entry.
     * @param id the identifier of the entry to retrieve.
     * @return a pointer to the entry on success; NULL otherwise.
     */
    const lru_cache_entry_t *lru_cache_get(lru_cache_t *const cache, const ulong id);
    /**
     * Replace an entry in the cache with the given data, or overwrite an
     * existing entry if id matches an existing identifier. The number of bytes
     * indicated by length and starting from *data are copied into the cache.
     * 
     * The new entry is promoted to the head of the usage list.
     * 
     * This function runs in O(log n + n + m) time in the worst case, where n
     * is the number of entries and m is size of added entry.
     * 
     * @param cache a pointer to the cache to populate.
     * @param id the identifier of the entry.
     * @param data a pointer to the data to store in the cache. Cannot be NULL
     *             unless length is zero (0).
     * @param length the number of bytes to copy into the cache. May not exceed
     *               the block size used when instantiating the cache.
     * @return a pointer to the new entry.
     */
    const lru_cache_entry_t *lru_cache_put(lru_cache_t *const cache, const ulong id,
                                           const char *data, const ulong length);
    
    /**
     * Return the entry with the lowest identifier.
     * 
     * The position of the entry in the usage list is not modified by this
     * function.
     * 
     * This function runs in O(1) time.
     * 
     * @param cache a pointer to the cache from which the entry is obtained.
     * @return a pointer to the first entry, or NULL if the cache is empty.
     */
    const lru_cache_entry_t *lru_cache_first(const lru_cache_t *const cache);
    /**
     * Return the entry with the highest identifier.
     * 
     * The position of the entry in the usage list is not modified by this
     * function.
     * 
     * This function runs in O(1) time.
     * 
     * @param cache a pointer to the cache from which the entry is obtained.
     * @return a pointer to the last entry, or NULL if the cache is empty.
     */
    const lru_cache_entry_t *lru_cache_last(const lru_cache_t *const cache);

#ifdef	__cplusplus
}
#endif

#endif	/* LRU_CACHE_H */

