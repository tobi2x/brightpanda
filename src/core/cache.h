#ifndef BRIGHTPANDA_CACHE_H
#define BRIGHTPANDA_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Cache manager - tracks file modification times and hashes
 * with LRU eviction policy when cache grows too large.
 */

#define CACHE_VERSION 1
#define DEFAULT_MAX_ENTRIES 50000     // 50k files
#define DEFAULT_MAX_BYTES (50 * 1024 * 1024)  // 50MB cache file

typedef struct {
    char* filepath;
    time_t mtime;           // Last modification time
    uint32_t hash;          // File content hash (CRC32)
    size_t size;            // File size in bytes
    time_t last_accessed;   // For LRU eviction
} CacheEntry;

typedef struct CacheManager CacheManager;
/* Hash table for fast lookups */
#define HASH_TABLE_SIZE 8192

typedef struct CacheNode {
    CacheEntry entry;
    struct CacheNode* next;
    struct CacheNode* lru_prev;  // LRU doubly-linked list
    struct CacheNode* lru_next;
} CacheNode;

struct CacheManager {
    char* cache_file;
    CacheNode* hash_table[HASH_TABLE_SIZE];
    size_t entry_count;
    size_t total_bytes;
    size_t hits;
    size_t misses;
    
    // LRU tracking
    CacheNode* lru_head;  // Most recently used
    CacheNode* lru_tail;  // Least recently used
    
    // Limits
    size_t max_entries;
    size_t max_bytes;
};

/* Create a cache manager with optional size limit (0 = unlimited) */
CacheManager* cache_manager_create(const char* cache_file);

/* Set cache size limits (0 = unlimited) */
void cache_set_limits(CacheManager* cache, size_t max_entries, size_t max_bytes);

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

/* Get cache size in bytes */
size_t cache_get_size_bytes(CacheManager* cache);

/* Free cache manager */
void cache_manager_free(CacheManager* cache);

#endif // BRIGHTPANDA_CACHE_H