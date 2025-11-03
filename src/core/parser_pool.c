#include "parser_pool.h"
#include "../util/logger.h"
#include <stdlib.h>
#include <string.h>

/* External Tree-sitter language declarations */
extern const TSLanguage *tree_sitter_python(void);

/* Pool state */
#define MAX_PARSERS 8

static struct {
    TSParser* parsers[MAX_PARSERS];
    bool in_use[MAX_PARSERS];
    const TSLanguage* language;
    size_t count;
    bool initialized;
} pool_state = {0};

bool parser_pool_init(void) {
    if (pool_state.initialized) {
        return true;
    }
    
    LOG_INFO("Initializing parser pool...");
    
    // Get Python language
    pool_state.language = tree_sitter_python();
    if (!pool_state.language) {
        LOG_ERROR("Failed to load Tree-sitter Python language");
        return false;
    }
    
    // Create initial parsers
    for (size_t i = 0; i < 2; i++) {  // Start with 2 parsers
        TSParser* parser = ts_parser_new();
        if (!parser) {
            LOG_ERROR("Failed to create parser");
            parser_pool_shutdown();
            return false;
        }
        
        if (!ts_parser_set_language(parser, pool_state.language)) {
            LOG_ERROR("Failed to set parser language");
            ts_parser_delete(parser);
            parser_pool_shutdown();
            return false;
        }
        
        pool_state.parsers[i] = parser;
        pool_state.in_use[i] = false;
        pool_state.count++;
    }
    
    pool_state.initialized = true;
    LOG_INFO("Parser pool initialized with %zu parsers", pool_state.count);
    
    return true;
}

TSParser* parser_pool_acquire(const char* language) {
    if (!pool_state.initialized) {
        if (!parser_pool_init()) {
            return NULL;
        }
    }
    
    // For now, only Python is supported
    if (language && strcmp(language, "python") != 0) {
        LOG_WARN("Unsupported language: %s", language);
        return NULL;
    }
    
    // Find an available parser
    for (size_t i = 0; i < pool_state.count; i++) {
        if (!pool_state.in_use[i]) {
            pool_state.in_use[i] = true;
            LOG_DEBUG("Acquired parser %zu from pool", i);
            return pool_state.parsers[i];
        }
    }
    
    // No available parser, create a new one if space allows
    if (pool_state.count < MAX_PARSERS) {
        TSParser* parser = ts_parser_new();
        if (!parser) {
            LOG_ERROR("Failed to create new parser");
            return NULL;
        }
        
        if (!ts_parser_set_language(parser, pool_state.language)) {
            LOG_ERROR("Failed to set parser language");
            ts_parser_delete(parser);
            return NULL;
        }
        
        size_t idx = pool_state.count;
        pool_state.parsers[idx] = parser;
        pool_state.in_use[idx] = true;
        pool_state.count++;
        
        LOG_DEBUG("Created new parser %zu (pool size: %zu)", idx, pool_state.count);
        return parser;
    }
    
    LOG_WARN("Parser pool exhausted");
    return NULL;
}

void parser_pool_release(TSParser* parser) {
    if (!parser) return;
    
    for (size_t i = 0; i < pool_state.count; i++) {
        if (pool_state.parsers[i] == parser) {
            pool_state.in_use[i] = false;
            LOG_DEBUG("Released parser %zu to pool", i);
            return;
        }
    }
    
    LOG_WARN("Released parser not from pool");
}

void parser_pool_shutdown(void) {
    if (!pool_state.initialized) {
        return;
    }
    
    LOG_INFO("Shutting down parser pool...");
    
    for (size_t i = 0; i < pool_state.count; i++) {
        if (pool_state.parsers[i]) {
            ts_parser_delete(pool_state.parsers[i]);
            pool_state.parsers[i] = NULL;
        }
    }
    
    pool_state.count = 0;
    pool_state.initialized = false;
    
    LOG_INFO("Parser pool shutdown complete");
}