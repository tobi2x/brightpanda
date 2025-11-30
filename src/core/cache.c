#include "cache.h"
#include "../util/logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

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

/* Simple hash function for filepath */
static uint32_t hash_filepath(const char* filepath) {
    uint32_t hash = 5381;
    int c;
    while ((c = *filepath++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

/* Move node to front of LRU list (most recently used) */
static void lru_touch(CacheManager* cache, CacheNode* node) {
    if (!cache || !node || node == cache->lru_head) {
        return;  // Already at front
    }
    
    // Remove from current position
    if (node->lru_prev) {
        node->lru_prev->lru_next = node->lru_next;
    }
    if (node->lru_next) {
        node->lru_next->lru_prev = node->lru_prev;
    }
    if (node == cache->lru_tail) {
        cache->lru_tail = node->lru_prev;
    }
    
    // Insert at head
    node->lru_prev = NULL;
    node->lru_next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->lru_prev = node;
    }
    cache->lru_head = node;
    
    if (!cache->lru_tail) {
        cache->lru_tail = node;
    }
}

/* Remove LRU node from list */
static void lru_remove(CacheManager* cache, CacheNode* node) {
    if (!cache || !node) return;
    
    if (node->lru_prev) {
        node->lru_prev->lru_next = node->lru_next;
    }
    if (node->lru_next) {
        node->lru_next->lru_prev = node->lru_prev;
    }
    if (node == cache->lru_head) {
        cache->lru_head = node->lru_next;
    }
    if (node == cache->lru_tail) {
        cache->lru_tail = node->lru_prev;
    }
    
    node->lru_prev = NULL;
    node->lru_next = NULL;
}

/* Evict least recently used entry */
static void cache_evict_lru(CacheManager* cache) {
    if (!cache || !cache->lru_tail) return;
    
    CacheNode* victim = cache->lru_tail;
    
    LOG_DEBUG("Evicting LRU entry: %s", victim->entry.filepath);
    
    // Remove from hash table
    uint32_t bucket = hash_filepath(victim->entry.filepath);
    CacheNode** indirect = &cache->hash_table[bucket];
    
    while (*indirect && *indirect != victim) {
        indirect = &(*indirect)->next;
    }
    
    if (*indirect) {
        *indirect = victim->next;
    }
    
    // Remove from LRU list
    lru_remove(cache, victim);
    
    // Update stats
    cache->entry_count--;
    cache->total_bytes -= sizeof(CacheEntry) + strlen(victim->entry.filepath);
    
    // Free memory
    free(victim->entry.filepath);
    free(victim);
}

/* Enforce cache limits by evicting LRU entries */
static void cache_enforce_limits(CacheManager* cache) {
    if (!cache) return;
    
    // Evict by entry count
    while (cache->max_entries > 0 && cache->entry_count > cache->max_entries) {
        cache_evict_lru(cache);
    }
    
    // Evict by total bytes
    while (cache->max_bytes > 0 && cache->total_bytes > cache->max_bytes) {
        cache_evict_lru(cache);
    }
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
    
    // Set default limits
    cache->max_entries = DEFAULT_MAX_ENTRIES;
    cache->max_bytes = DEFAULT_MAX_BYTES;
    
    return cache;
}

void cache_set_limits(CacheManager* cache, size_t max_entries, size_t max_bytes) {
    if (!cache) return;
    
    cache->max_entries = max_entries;
    cache->max_bytes = max_bytes;
    
    LOG_DEBUG("Cache limits set: %zu entries, %zu bytes", max_entries, max_bytes);
    
    // Enforce limits immediately
    cache_enforce_limits(cache);
}

bool cache_manager_load(CacheManager* cache) {
    if (!cache || !cache->cache_file) return false;
    
    FILE* file = fopen(cache->cache_file, "r");
    if (!file) {
        LOG_DEBUG("Cache file not found, starting fresh: %s", cache->cache_file);
        return true; // Not an error
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
    
    size_t loaded = 0;
    
    // Read entries
    for (size_t i = 0; i < count; i++) {
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
        time_t mtime, last_accessed;
        uint32_t hash;
        size_t size;
        
        if (fread(&mtime, sizeof(mtime), 1, file) != 1 ||
            fread(&hash, sizeof(hash), 1, file) != 1 ||
            fread(&size, sizeof(size), 1, file) != 1 ||
            fread(&last_accessed, sizeof(last_accessed), 1, file) != 1) {
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
        node->entry.last_accessed = last_accessed;
        
        node->next = cache->hash_table[bucket];
        cache->hash_table[bucket] = node;
        
        // Add to LRU list (at tail, since we're loading in order)
        if (cache->lru_tail) {
            cache->lru_tail->lru_next = node;
            node->lru_prev = cache->lru_tail;
        } else {
            cache->lru_head = node;
        }
        cache->lru_tail = node;
        
        cache->entry_count++;
        cache->total_bytes += sizeof(CacheEntry) + strlen(filepath);
        loaded++;
    }
    
    fclose(file);
    
    LOG_INFO("Loaded %zu cache entries from %s (%.2f MB)", 
             loaded, cache->cache_file, cache->total_bytes / (1024.0 * 1024.0));
    
    // Enforce limits after loading
    cache_enforce_limits(cache);
    
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
            fwrite(&node->entry.last_accessed, sizeof(node->entry.last_accessed), 1, file);
            
            node = node->next;
        }
    }
    
    fclose(file);
    
    LOG_INFO("Saved %zu cache entries to %s (%.2f MB)", 
             cache->entry_count, cache->cache_file, cache->total_bytes / (1024.0 * 1024.0));
    
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
            node->entry.last_accessed = time(NULL);
            lru_touch(cache, node);
            
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
            node->entry.last_accessed = time(NULL);
            lru_touch(cache, node);
            LOG_DEBUG("Updated cache entry: %s", filepath);
            return true;
        }
        node = node->next;
    }
    
    // Add new entry
    node = calloc(1, sizeof(CacheNode));
    if (!node) return false;
    
    node->entry.filepath = strdup(filepath);
    node->entry.mtime = st.st_mtime;
    node->entry.hash = hash;
    node->entry.size = st.st_size;
    node->entry.last_accessed = time(NULL);
    
    node->next = cache->hash_table[bucket];
    cache->hash_table[bucket] = node;
    
    // Add to LRU list (at head - most recently used)
    node->lru_next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->lru_prev = node;
    }
    cache->lru_head = node;
    if (!cache->lru_tail) {
        cache->lru_tail = node;
    }
    
    cache->entry_count++;
    cache->total_bytes += sizeof(CacheEntry) + strlen(filepath);
    
    LOG_DEBUG("Added cache entry: %s", filepath);
    
    // Enforce limits
    cache_enforce_limits(cache);
    
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
    
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->entry_count = 0;
    cache->total_bytes = 0;
    cache->hits = 0;
    cache->misses = 0;
}

void cache_get_stats(CacheManager* cache, size_t* total_entries, size_t* hits, size_t* misses) {
    if (!cache) return;
    
    if (total_entries) *total_entries = cache->entry_count;
    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
}

size_t cache_get_size_bytes(CacheManager* cache) {
    return cache ? cache->total_bytes : 0;
}

void cache_manager_free(CacheManager* cache) {
    if (!cache) return;
    
    cache_clear(cache);
    free(cache->cache_file);
    free(cache);
}