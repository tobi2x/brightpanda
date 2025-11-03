#include "extractor.h"
#include "../util/logger.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 

bool extractor_execute_query(
    TSQuery* query,
    TSTree* tree,
    const char* source,
    ExtractorMatchCallback callback,
    void* userdata
) {
    if (!query || !tree || !source || !callback) {
        return false;
    }
    
    TSNode root_node = ts_tree_root_node(tree);
    TSQueryCursor* cursor = ts_query_cursor_new();
    
    ts_query_cursor_exec(cursor, query, root_node);
    
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        callback(match, query, source, userdata);
    }
    
    ts_query_cursor_delete(cursor);
    return true;
}

char* extractor_get_node_text(TSNode node, const char* source) {
    if (!source) return NULL;
    
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    uint32_t length = end - start;
    
    if (length == 0) return NULL;
    
    char* text = malloc(length + 1);
    if (!text) return NULL;
    
    memcpy(text, source + start, length);
    text[length] = '\0';
    
    return text;
}

char* extractor_get_capture_text(
    TSQueryMatch match,
    uint16_t capture_index,
    const char* source
) {
    if (capture_index >= match.capture_count) {
        return NULL;
    }
    
    TSNode node = match.captures[capture_index].node;
    return extractor_get_node_text(node, source);
}

const char* extractor_get_capture_name(TSQuery* query, uint32_t capture_id) {
    if (!query) return NULL;
    
    uint32_t length;
    return ts_query_capture_name_for_id(query, capture_id, &length);
}

bool extractor_find_capture(
    TSQueryMatch match,
    TSQuery* query,
    const char* capture_name,
    TSNode* out_node
) {
    if (!query || !capture_name) return false;
    
    for (uint16_t i = 0; i < match.capture_count; i++) {
        TSQueryCapture capture = match.captures[i];
        
        uint32_t length;
        const char* name = ts_query_capture_name_for_id(query, capture.index, &length);
        
        if (strncmp(name, capture_name, length) == 0 && strlen(capture_name) == length) {
            if (out_node) {
                *out_node = capture.node;
            }
            return true;
        }
    }
    
    return false;
}

char* extractor_strip_quotes(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    if (len < 2) return strdup(str);
    
    // Check if starts and ends with quotes
    if ((str[0] == '"' || str[0] == '\'') && str[len-1] == str[0]) {
        char* result = malloc(len - 1);
        if (!result) return NULL;
        memcpy(result, str + 1, len - 2);
        result[len - 2] = '\0';
        return result;
    }
    
    return strdup(str);
}

/**
 * Extracts HTTP method from a query match.
 * Supports Flask (methods keyword) and FastAPI (decorator method)
 * Defaults to "GET" if nothing found.
 */
char* extractor_get_http_method(TSQueryMatch match, TSQuery* query, const char* source) {
    TSNode node;
    char* raw = NULL;

    // Case 1: FastAPI - @app.post("/...")
    if (extractor_find_capture(match, query, "fastapi.method", &node)) {
        raw = extractor_get_node_text(node, source);
        if (raw) {
            // .post -> POST
            for (char* p = raw; *p; ++p) *p = toupper(*p);
            return raw;
        }
    }

    // Case 2: Flask - methods=['POST']
    if (extractor_find_capture(match, query, "route.method", &node)) {
        raw = extractor_get_node_text(node, source);
        if (raw) {
            char* clean = extractor_strip_quotes(raw);
            free(raw);
            for (char* p = clean; *p; ++p) *p = toupper(*p);
            return clean;
        }
    }

    // Default to GET
    return strdup("GET");
}
