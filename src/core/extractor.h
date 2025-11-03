#ifndef BRIGHTPANDA_EXTRACTOR_H
#define BRIGHTPANDA_EXTRACTOR_H

#include <tree_sitter/api.h>
#include <stdbool.h>

/*
 * Tree-sitter query extractor - shared utilities for executing queries
 * and extracting text from AST nodes.
 */

/* Callback invoked for each query match */
typedef void (*ExtractorMatchCallback)(
    TSQueryMatch match,
    TSQuery* query,
    const char* source,
    void* userdata
);

/* Execute a Tree-sitter query on a tree and invoke callback for each match */
bool extractor_execute_query(
    TSQuery* query,
    TSTree* tree,
    const char* source,
    ExtractorMatchCallback callback,
    void* userdata
);

/* Get the text for a captured node */
char* extractor_get_node_text(
    TSNode node,
    const char* source
);

/* Get the text for a capture by index in a match */
char* extractor_get_capture_text(
    TSQueryMatch match,
    uint16_t capture_index,
    const char* source
);

/* Get the name of a capture by its index */
const char* extractor_get_capture_name(
    TSQuery* query,
    uint32_t capture_id
);

/* Find a capture by name in a match */
bool extractor_find_capture(
    TSQueryMatch match,
    TSQuery* query,
    const char* capture_name,
    TSNode* out_node
);

/* Strip quotes from a string literal */
char* extractor_strip_quotes(const char* str);

/* Extract HTTP method (GET/POST/PUT/DELETE...) from match or default to GET */
char* extractor_get_http_method(
    TSQueryMatch match,
    TSQuery* query,
    const char* source
);


#endif // BRIGHTPANDA_EXTRACTOR_H