#include "lru_cache.h"
#include <assert.h>

typedef struct blk {
    lru_cache_entry_t payload;
    struct blk *next;
    struct blk *previous;
} block;

struct lru_cache {
    block *blocks;
    block *head;
    block *tail;
    ulong cur_blocks;
    ulong max_blocks;
    ulong block_size;
};

static long find_block(const lru_cache_t *const cache, const ulong id);


lru_cache_t *lru_cache_init(const ulong blocks, const ulong block_size) {
    lru_cache_t *cache = malloc(sizeof *cache);
    assert(blocks != 0);
    assert(block_size != 0);
    
    if (cache != NULL) {
        char *data = malloc(blocks * block_size);
        cache->block_size = block_size;
        cache->blocks = malloc(blocks * sizeof(block));
        
        if ((data != NULL) && (cache->blocks != NULL)) {
            ulong i;
            for (i = 0; i < blocks; i++) {
                cache->blocks[i].payload.id = 0;
                cache->blocks[i].payload.data = data;
                cache->blocks[i].payload.size = 0;
                cache->blocks[i].next = NULL;
                cache->blocks[i].previous = NULL;
                data += block_size;
            }
            cache->head = NULL;
            cache->tail = NULL;
            cache->cur_blocks = 0;
            cache->max_blocks = blocks;
        } else {
            // As data will be the larger allocation, it's possible for it to
            // fail but for the block array allocation to succeed so free both.
            if (data != NULL)
                free(data);
            if (cache->blocks != NULL)
                free(cache->blocks);
            free(cache);
            cache = NULL;
        }
    }
    return cache;
}


void lru_cache_free(lru_cache_t *const cache) {
    char *data;
    ulong i;
    if (cache == NULL)
        return;

    // Find the start of the data region. As the blocks may have changed order,
    // this may no longer be the first block in the array.
    data = cache->blocks[0].payload.data;
    for (i = 1; i < cache->cur_blocks; i++)
        if(cache->blocks[i].payload.data < data)
            data = cache->blocks[i].payload.data;

    free(data);
    free(cache->blocks);
    free(cache);
}


const lru_cache_entry_t *lru_cache_get(lru_cache_t *const cache, const ulong id) {
    const long i = find_block(cache, id);
    block *block;
    if (i < 0)
        return NULL;
    
    block = &cache->blocks[i];
    if (block != cache->head) {
        // Remove the block from its current location in the last used list...
        if (block->previous != NULL)
            block->previous->next = block->next;
        if (block->next != NULL)
            block->next->previous = block->previous;
        // ...reassigning the list tail if necessary...
        if (block == cache->tail)
            cache->tail = block->previous;
        // ...and move it to the head of the list.
        block->previous = NULL;
        block->next = cache->head;
        cache->head->previous = block;
        cache->head = block;
    }
    return &block->payload;
}


const lru_cache_entry_t *lru_cache_put(lru_cache_t *const cache, const ulong id, const char *data,
                                       const ulong length) {
    long i = find_block(cache, id);
    block *blk;
    assert((data != NULL) || (length == 0));
    assert(length <= cache->block_size);
    
    if (i < 0) {
        i = -(i + 1);
        if (i == cache->max_blocks)
            i = cache->max_blocks - 1;
        blk = &cache->blocks[i];
        
        if (cache->cur_blocks == cache->max_blocks) {
            ptrdiff_t j = cache->tail - blk + i;
            const int dir = (cache->tail < blk) ? 1 : -1;
            char *stale_data = cache->tail->payload.data;
            cache->tail->previous->next = NULL;
            cache->tail = cache->tail->previous;
            while (j < i) {
                // Shift block...
                cache->blocks[j] = cache->blocks[j + dir];
                // ...and adjust list pointers.
                if (cache->blocks[j].previous != NULL)
                    cache->blocks[j].previous->next = &cache->blocks[j];
                if (cache->blocks[j].next != NULL)
                    cache->blocks[j].next->previous = &cache->blocks[j];
                if (cache->head == &cache->blocks[j + dir])
                    cache->head = &cache->blocks[j];
                if (cache->tail == &cache->blocks[j + dir])
                    cache->tail = &cache->blocks[j];
                j += dir;
            }
            blk->payload.data = stale_data;
            blk->next = cache->head;
            blk->previous = NULL;
            cache->head->previous = blk;
        } else {
            if (cache->cur_blocks > 0) {
                block *new_block = &cache->blocks[cache->cur_blocks];
                ptrdiff_t j = new_block - blk + i;
                char *stale_data = new_block->payload.data;
                while (j > i) {
                    // Shift block...
                    cache->blocks[j] = cache->blocks[j - 1];
                    // ...and adjust list pointers.
                    if (cache->blocks[j].previous != NULL)
                        cache->blocks[j].previous->next = &cache->blocks[j];
                    if (cache->blocks[j].next != NULL)
                        cache->blocks[j].next->previous = &cache->blocks[j];
                    if (cache->head == &cache->blocks[j - 1])
                        cache->head = &cache->blocks[j];
                    if (cache->tail == &cache->blocks[j - 1])
                        cache->tail = &cache->blocks[j];
                    j--;
                }
                blk->payload.data = stale_data;
                blk->next = cache->head;
                cache->head->previous = blk;
            } else {
                cache->tail = blk;
            }
            cache->cur_blocks++;
        }
/*
        if ((BLOCKID(*new_block) != id) || (cache->cur_blocks == 0)) {
            block *stale_block;
            // If the cache is not yet full, include the next free block.
            // Otherwise, let the block in the tail position of the usage list be
            // overwritten.
            if (cache->cur_blocks < cache->max_blocks) {
                stale_block = &cache->blocks[cache->cur_blocks++];
            } else {
                // If we are overwriting the tail position, it will eventually be moved
                // to the head of the usage list, so terminate the forward links at its
                // predecessor.
                if (cache->tail->previous != NULL)
                    cache->tail->previous->next = NULL;
                stale_block = cache->tail;
            }
            // If we're lucky, the old block will be the same as the insertion
            // point, but it's more likely that intervening blocks will need to be
            // shifted.
            if (stale_block != new_block) {
                const int dir = (stale_block < new_block) ? 1 : -1;
                ptrdiff_t j = i + stale_block - new_block;
                // Before the old block is overwritten, save its data address (this
                // is where the new data will go).
                char *const data_ptr = BLOCKDATA(*stale_block);
                // If it's the tail, move its predecessor into that position.
                if (stale_block == cache->tail)
                    cache->tail = cache->tail->previous;

                while (i > j) {
                    cache->blocks[j] = cache->blocks[j + dir];
                    j += dir;
                }

                // Update the data address for the now-empty block
                BLOCKDATA(*new_block) = data_ptr;
            }
        } //else {
            // Remove the existing block from its current location in the last used
            // list.
            if (new_block->previous != NULL)
                new_block->previous->next = new_block->next;
            if (new_block->next != NULL)
                new_block->next->previous = new_block->previous;
        //}
        // Move the block to the head of the list...
        new_block->previous = NULL;
        new_block->next = cache->head;
        if (cache->head != NULL)
            cache->head->previous = new_block;
        cache->head = new_block;
        // ...and overwrite its data
        memcpy(BLOCKDATA(*new_block), data, length);
        BLOCKID(*new_block) = id;
        BLOCKSIZE(*new_block) = length;
        return &new_block->payload;
 */
    } else {
        // It's expected that put calls will be to insert new data into the
        // cache, but there's no reason not to allow the replacement of data in
        // existing blocks.
        blk = &cache->blocks[i];
        if (blk != cache->head) {
            // Remove the block from its current location in the last used list...
            if (blk->previous != NULL)
                blk->previous->next = blk->next;
            if (blk->next != NULL)
                blk->next->previous = blk->previous;
            // ...reassigning the list tail if necessary...
            if (blk == cache->tail)
                cache->tail = blk->previous;
            // ...and move it to the head of the list.
            blk->previous = NULL;
            blk->next = cache->head;
            cache->head->previous = blk;
        }
    }
    // Move the block to the head of the list...
    cache->head = blk;
    // ...and overwrite its data.
    memcpy(blk->payload.data, data, length);
    blk->payload.id = id;
    blk->payload.size = length;
    return &blk->payload;
}


const lru_cache_entry_t *lru_cache_first(const lru_cache_t *const cache) {
    return (cache->cur_blocks > 0) ? &cache->blocks[0].payload : NULL;
}


const lru_cache_entry_t *lru_cache_last(const lru_cache_t *const cache) {
    return (cache->cur_blocks > 0) ? &cache->blocks[cache->cur_blocks - 1].payload : NULL;
}


static long find_block(const lru_cache_t *const cache, const ulong id) {
    long low = 0;
    long high = cache->cur_blocks - 1;
        
    while (low <= high) {
        const long mid = (low + high) / 2;
        const ulong found_id = cache->blocks[mid].payload.id;

        if (found_id < id)
            low = mid + 1;
        else if (found_id > id)
            high = mid - 1;
        else
            return mid;
    }
    return -low - 1;
}
