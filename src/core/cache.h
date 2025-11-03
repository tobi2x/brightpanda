#ifndef BRIGHTPANDA_CACHE_H
#define BRIGHTPANDA_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Cache manager - tracks file modification times and hashes
 * to skip parsing unchanged files on incremental scans.
 */

typedef struct {
    char* filepath;
    time_t mtime;           // Last modification time
    uint32_t hash;          // File content hash (CRC32)
    size_t size;            // File size in bytes
} CacheEntry;

typedef struct CacheManager CacheManager;

/* Create a cache manager */
CacheManager* cache_manager_create(const char* cache_file);

/* Load cache from disk */
bool cache_manager_load(CacheManager* cache);

/* Save cache to disk */
bool cache_manager_save(CacheManager* cache);

/* Check if a file has changed since last scan */
bool cache_is_file_changed(CacheManager* cache, const char* filepath);

/* Update cache entry for a file */
bool cache_update_file(CacheManager* cache, const char* filepath);

/* Clear all cache entries */
void cache_clear(CacheManager* cache);

/* Get cache statistics */
void cache_get_stats(CacheManager* cache, size_t* total_entries, size_t* hits, size_t* misses);

/* Free cache manager */
void cache_manager_free(CacheManager* cache);

#endif // BRIGHTPANDA_CACHE_H