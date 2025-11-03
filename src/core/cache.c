#include "cache.h"
#include "../util/logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#define CACHE_VERSION 1
#define MAX_CACHE_ENTRIES 10000

/* Simple CRC32 implementation */
static uint32_t crc32(const char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint8_t)data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

/* Hash table for fast lookups */
#define HASH_TABLE_SIZE 4096

typedef struct CacheNode {
    CacheEntry entry;
    struct CacheNode* next;
} CacheNode;

struct CacheManager {
    char* cache_file;
    CacheNode* hash_table[HASH_TABLE_SIZE];
    size_t entry_count;
    size_t hits;
    size_t misses;
};

/* Simple hash function for filepath */
static uint32_t hash_filepath(const char* filepath) {
    uint32_t hash = 5381;
    int c;
    while ((c = *filepath++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

CacheManager* cache_manager_create(const char* cache_file) {
    if (!cache_file) return NULL;
    
    CacheManager* cache = calloc(1, sizeof(CacheManager));
    if (!cache) return NULL;
    
    cache->cache_file = strdup(cache_file);
    if (!cache->cache_file) {
        free(cache);
        return NULL;
    }
    
    return cache;
}

bool cache_manager_load(CacheManager* cache) {
    if (!cache || !cache->cache_file) return false;
    
    FILE* file = fopen(cache->cache_file, "r");
    if (!file) {
        LOG_DEBUG("Cache file not found, starting fresh: %s", cache->cache_file);
        return true; // Not an error, just no cache yet
    }
    
    // Read version
    uint32_t version;
    if (fread(&version, sizeof(version), 1, file) != 1 || version != CACHE_VERSION) {
        LOG_WARN("Cache file version mismatch, ignoring");
        fclose(file);
        return true;
    }
    
    // Read entry count
    size_t count;
    if (fread(&count, sizeof(count), 1, file) != 1) {
        LOG_ERROR("Failed to read cache entry count");
        fclose(file);
        return false;
    }
    
    LOG_DEBUG("Loading %zu cache entries...", count);
    
    // Read entries
    for (size_t i = 0; i < count && i < MAX_CACHE_ENTRIES; i++) {
        // Read filepath length
        uint16_t path_len;
        if (fread(&path_len, sizeof(path_len), 1, file) != 1) break;
        
        // Read filepath
        char* filepath = malloc(path_len + 1);
        if (!filepath) break;
        
        if (fread(filepath, 1, path_len, file) != path_len) {
            free(filepath);
            break;
        }
        filepath[path_len] = '\0';
        
        // Read entry data
        time_t mtime;
        uint32_t hash;
        size_t size;
        
        if (fread(&mtime, sizeof(mtime), 1, file) != 1 ||
            fread(&hash, sizeof(hash), 1, file) != 1 ||
            fread(&size, sizeof(size), 1, file) != 1) {
            free(filepath);
            break;
        }
        
        // Add to hash table
        uint32_t bucket = hash_filepath(filepath);
        CacheNode* node = calloc(1, sizeof(CacheNode));
        if (!node) {
            free(filepath);
            continue;
        }
        
        node->entry.filepath = filepath;
        node->entry.mtime = mtime;
        node->entry.hash = hash;
        node->entry.size = size;
        
        node->next = cache->hash_table[bucket];
        cache->hash_table[bucket] = node;
        cache->entry_count++;
    }
    
    fclose(file);
    LOG_INFO("Loaded %zu cache entries from %s", cache->entry_count, cache->cache_file);
    
    return true;
}

bool cache_manager_save(CacheManager* cache) {
    if (!cache || !cache->cache_file) return false;
    
    FILE* file = fopen(cache->cache_file, "w");
    if (!file) {
        LOG_ERROR("Failed to open cache file for writing: %s", cache->cache_file);
        return false;
    }
    
    // Write version
    uint32_t version = CACHE_VERSION;
    fwrite(&version, sizeof(version), 1, file);
    
    // Write entry count
    fwrite(&cache->entry_count, sizeof(cache->entry_count), 1, file);
    
    // Write all entries
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        CacheNode* node = cache->hash_table[i];
        while (node) {
            uint16_t path_len = strlen(node->entry.filepath);
            fwrite(&path_len, sizeof(path_len), 1, file);
            fwrite(node->entry.filepath, 1, path_len, file);
            fwrite(&node->entry.mtime, sizeof(node->entry.mtime), 1, file);
            fwrite(&node->entry.hash, sizeof(node->entry.hash), 1, file);
            fwrite(&node->entry.size, sizeof(node->entry.size), 1, file);
            
            node = node->next;
        }
    }
    
    fclose(file);
    LOG_INFO("Saved %zu cache entries to %s", cache->entry_count, cache->cache_file);
    
    return true;
}

bool cache_is_file_changed(CacheManager* cache, const char* filepath) {
    if (!cache || !filepath) return true;
    
    // Get file stats
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return true; // File doesn't exist or can't be read
    }
    
    // Look up in cache
    uint32_t bucket = hash_filepath(filepath);
    CacheNode* node = cache->hash_table[bucket];
    
    while (node) {
        if (strcmp(node->entry.filepath, filepath) == 0) {
            // Found in cache - check if changed
            if (node->entry.mtime == st.st_mtime && node->entry.size == st.st_size) {
                // Quick check: mtime and size match
                cache->hits++;
                LOG_DEBUG("Cache hit: %s", filepath);
                return false; // Not changed
            } else {
                // File modified
                cache->misses++;
                LOG_DEBUG("Cache miss (modified): %s", filepath);
                return true;
            }
        }
        node = node->next;
    }
    
    // Not in cache
    cache->misses++;
    LOG_DEBUG("Cache miss (new file): %s", filepath);
    return true;
}

bool cache_update_file(CacheManager* cache, const char* filepath) {
    if (!cache || !filepath) return false;
    
    // Get file stats
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return false;
    }
    
    // Read file for hash
    FILE* file = fopen(filepath, "rb");
    if (!file) return false;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0 || size > 10 * 1024 * 1024) { // 10MB limit
        fclose(file);
        return false;
    }
    
    char* content = malloc(size);
    if (!content) {
        fclose(file);
        return false;
    }
    
    size_t bytes_read = fread(content, 1, size, file);
    fclose(file);
    
    uint32_t hash = crc32(content, bytes_read);
    free(content);
    
    // Check if already exists
    uint32_t bucket = hash_filepath(filepath);
    CacheNode* node = cache->hash_table[bucket];
    
    while (node) {
        if (strcmp(node->entry.filepath, filepath) == 0) {
            // Update existing entry
            node->entry.mtime = st.st_mtime;
            node->entry.hash = hash;
            node->entry.size = st.st_size;
            LOG_DEBUG("Updated cache entry: %s", filepath);
            return true;
        }
        node = node->next;
    }
    
    // Add new entry
    if (cache->entry_count >= MAX_CACHE_ENTRIES) {
        LOG_WARN("Cache full, not adding: %s", filepath);
        return false;
    }
    
    node = calloc(1, sizeof(CacheNode));
    if (!node) return false;
    
    node->entry.filepath = strdup(filepath);
    node->entry.mtime = st.st_mtime;
    node->entry.hash = hash;
    node->entry.size = st.st_size;
    
    node->next = cache->hash_table[bucket];
    cache->hash_table[bucket] = node;
    cache->entry_count++;
    
    LOG_DEBUG("Added cache entry: %s", filepath);
    
    return true;
}

void cache_clear(CacheManager* cache) {
    if (!cache) return;
    
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        CacheNode* node = cache->hash_table[i];
        while (node) {
            CacheNode* next = node->next;
            free(node->entry.filepath);
            free(node);
            node = next;
        }
        cache->hash_table[i] = NULL;
    }
    
    cache->entry_count = 0;
    cache->hits = 0;
    cache->misses = 0;
}

void cache_get_stats(CacheManager* cache, size_t* total_entries, size_t* hits, size_t* misses) {
    if (!cache) return;
    
    if (total_entries) *total_entries = cache->entry_count;
    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
}

void cache_manager_free(CacheManager* cache) {
    if (!cache) return;
    
    cache_clear(cache);
    free(cache->cache_file);
    free(cache);
}