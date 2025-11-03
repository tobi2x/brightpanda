#include "../plugin.h"
#include "../../core/parser_pool.h"
#include "../../util/logger.h"
#include "../../util/path.h"
#include <tree_sitter/api.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External Tree-sitter language */
extern const TSLanguage *tree_sitter_python(void);

/* Python plugin state */
static struct {
    bool initialized;
    TSQuery* functions_query;
    TSQuery* routes_query;
    TSQuery* calls_query;
} python_state = {0};

/* Query capture indices - these correspond to the @names in .scm files */
typedef struct {
    uint32_t function_name;
    uint32_t decorator_name;
    uint32_t route_path;
    uint32_t route_method;
    uint32_t route_handler;
    uint32_t http_client_method;
    uint32_t http_client_url;
    uint32_t import_module;
} CaptureIndices;

static CaptureIndices capture_indices = {0};

/* Forward declarations */
static bool python_init(void);
static void python_shutdown(void);
static bool python_supports_file(const char* filepath);
static ParseResult* python_parse_file(const char* filepath, const char* service_name);
static const char* python_get_query_path(const char* query_name);
static char* python_infer_service_name(const char* filepath);

/* Helper functions */
static char* read_file_contents(const char* filepath);
static TSQuery* load_query_from_string(const TSLanguage* language, const char* query_string);
static void extract_routes(TSTree* tree, const char* source, ParseResult* result, const char* service_name);
static void extract_calls(TSTree* tree, const char* source, ParseResult* result, const char* service_name);
static void extract_imports(TSTree* tree, const char* source, ParseResult* result);

/* Supported file extensions */
static const char* python_extensions[] = {
    "py",
    "pyi",
    NULL
};

/* Plugin definition */
static LanguagePlugin python_plugin = {
    .name = "python",
    .version = "1.0.0",
    .file_extensions = python_extensions,
    .init = python_init,
    .shutdown = python_shutdown,
    .supports_file = python_supports_file,
    .parse_file = python_parse_file,
    .get_query_path = python_get_query_path,
    .infer_service_name = python_infer_service_name
};

LanguagePlugin* python_plugin_create(void) {
    return &python_plugin;
}

static bool python_init(void) {
    if (python_state.initialized) {
        return true;
    }
    
    LOG_INFO("Initializing Python plugin...");
    
    // Initialize parser pool
    if (!parser_pool_init()) {
        LOG_ERROR("Failed to initialize parser pool");
        return false;
    }
    
    const TSLanguage* language = tree_sitter_python();
    
    // Load queries - for MVP, we'll use inline queries
    // In production, these would be loaded from .scm files
    
    // Routes query (Flask/FastAPI)
    const char* routes_query_string = 
        "(decorated_definition\n"
        "  (decorator\n"
        "    (call\n"
        "      function: (attribute\n"
        "        attribute: (identifier) @route.decorator)\n"
        "      arguments: (argument_list\n"
        "        (string) @route.path)))\n"
        "  definition: (function_definition\n"
        "    name: (identifier) @route.handler))\n";
    
    python_state.routes_query = load_query_from_string(language, routes_query_string);
    
    // HTTP calls query
    const char* calls_query_string =
        "(call\n"
        "  function: (attribute\n"
        "    object: (identifier) @http.client.lib\n"
        "    attribute: (identifier) @http.client.method)\n"
        "  arguments: (argument_list\n"
        "    (string) @http.client.url))\n";
    
    python_state.calls_query = load_query_from_string(language, calls_query_string);
    
    // Imports query
    const char* imports_query_string =
        "(import_statement\n"
        "  name: (dotted_name) @import.module)\n"
        "(import_from_statement\n"
        "  module_name: (dotted_name) @import.from.module)\n";
    
    python_state.functions_query = load_query_from_string(language, imports_query_string);
    
    python_state.initialized = true;
    LOG_INFO("Python plugin initialized successfully");
    
    return true;
}

static void python_shutdown(void) {
    if (!python_state.initialized) {
        return;
    }
    
    LOG_DEBUG("Shutting down Python plugin...");
    
    if (python_state.functions_query) {
        ts_query_delete(python_state.functions_query);
        python_state.functions_query = NULL;
    }
    
    if (python_state.routes_query) {
        ts_query_delete(python_state.routes_query);
        python_state.routes_query = NULL;
    }
    
    if (python_state.calls_query) {
        ts_query_delete(python_state.calls_query);
        python_state.calls_query = NULL;
    }
    
    parser_pool_shutdown();
    
    python_state.initialized = false;
}

static bool python_supports_file(const char* filepath) {
    if (!filepath) return false;
    
    const char* ext = path_get_extension(filepath);
    if (!ext) return false;
    
    for (size_t i = 0; python_extensions[i]; i++) {
        if (strcmp(ext, python_extensions[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

static ParseResult* python_parse_file(const char* filepath, const char* service_name) {
    if (!filepath) return NULL;
    
    if (!python_state.initialized) {
        if (!python_init()) {
            return NULL;
        }
    }
    
    LOG_DEBUG("Parsing Python file: %s", filepath);
    
    ParseResult* result = parse_result_create();
    if (!result) return NULL;
    
    // Read file contents
    char* source = read_file_contents(filepath);
    if (!source) {
        result->error_message = strdup("Failed to read file");
        result->success = false;
        return result;
    }
    
    // Get parser from pool
    TSParser* parser = parser_pool_acquire("python");
    if (!parser) {
        result->error_message = strdup("Failed to acquire parser");
        result->success = false;
        free(source);
        return result;
    }
    
    // Parse the file
    TSTree* tree = ts_parser_parse_string(parser, NULL, source, strlen(source));
    if (!tree) {
        result->error_message = strdup("Failed to parse file");
        result->success = false;
        parser_pool_release(parser);
        free(source);
        return result;
    }
    
    TSNode root_node = ts_tree_root_node(tree);
    
    // Check for syntax errors
    if (ts_node_has_error(root_node)) {
        LOG_WARN("Syntax errors in file: %s", filepath);
        // Continue anyway - we can still extract partial information
    }
    
    // Extract information
    if (!service_name) {
        service_name = python_infer_service_name(filepath);
    }
    
    result->service = service_create(service_name ? service_name : "unknown", "python", filepath);
    
    extract_routes(tree, source, result, service_name);
    extract_calls(tree, source, result, service_name);
    extract_imports(tree, source, result);
    
    result->success = true;
    
    // Cleanup
    ts_tree_delete(tree);
    parser_pool_release(parser);
    free(source);
    
    LOG_DEBUG("Python parsing complete: %zu endpoints, %zu edges, %zu imports",
              result->endpoints->count, result->edges->count, result->import_count);
    
    return result;
}

static const char* python_get_query_path(const char* query_name) {
    // For MVP, queries are inline
    // In production, return path like: "src/lang/python/queries/routes.scm"
    (void)query_name;
    return NULL;
}

static char* python_infer_service_name(const char* filepath) {
    if (!filepath) return NULL;
    
    // Simple heuristic: use parent directory name
    char* dir = path_dirname(filepath);
    if (!dir) return strdup("unknown");
    
    const char* basename = path_basename(dir);
    char* service_name = strdup(basename ? basename : "unknown");
    
    free(dir);
    return service_name;
}

/* Helper function implementations */

static char* read_file_contents(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", filepath);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0 || size > 10 * 1024 * 1024) {  // 10MB limit
        LOG_ERROR("File too large or invalid: %s", filepath);
        fclose(file);
        return NULL;
    }
    
    // Read contents
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    return buffer;
}

static TSQuery* load_query_from_string(const TSLanguage* language, const char* query_string) {
    uint32_t error_offset;
    TSQueryError error_type;
    
    TSQuery* query = ts_query_new(language, query_string, strlen(query_string), &error_offset, &error_type);
    
    if (!query) {
        LOG_ERROR("Failed to create query at offset %u: error type %d", error_offset, error_type);
        return NULL;
    }
    
    return query;
}

static void extract_routes(TSTree* tree, const char* source, ParseResult* result, const char* service_name) {
    if (!python_state.routes_query) return;
    
    TSNode root_node = ts_tree_root_node(tree);
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, python_state.routes_query, root_node);
    
    TSQueryMatch match;
    char path_buffer[512];
    char handler_buffer[256];
    
    while (ts_query_cursor_next_match(cursor, &match)) {
        const char* route_path = NULL;
        const char* handler_name = NULL;
        
        for (uint16_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            uint32_t capture_name_len;
            const char* capture_name = ts_query_capture_name_for_id(
                python_state.routes_query, capture.index, &capture_name_len);
            
            TSNode node = capture.node;
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            uint32_t length = end - start;
            
            if (strncmp(capture_name, "route.path", capture_name_len) == 0) {
                if (length < sizeof(path_buffer) - 1) {
                    memcpy(path_buffer, source + start, length);
                    path_buffer[length] = '\0';
                    // Strip quotes from string literal
                    if (path_buffer[0] == '"' || path_buffer[0] == '\'') {
                        route_path = path_buffer + 1;
                        if (length > 2) path_buffer[length - 1] = '\0';
                    } else {
                        route_path = path_buffer;
                    }
                }
            } else if (strncmp(capture_name, "route.handler", capture_name_len) == 0) {
                if (length < sizeof(handler_buffer)) {
                    memcpy(handler_buffer, source + start, length);
                    handler_buffer[length] = '\0';
                    handler_name = handler_buffer;
                }
            }
        }
        
        // Create endpoint if we found both path and handler
        if (route_path && handler_name && service_name) {
            Endpoint* endpoint = endpoint_create(
                service_name,
                route_path,
                HTTP_GET,  // Default to GET, would need more sophisticated parsing for methods
                handler_name,
                path_basename(result->service->path),
                ts_node_start_point(match.captures[0].node).row + 1
            );
            
            if (endpoint) {
                endpoint_list_add(result->endpoints, endpoint);
                LOG_DEBUG("Found endpoint: %s %s -> %s", 
                         http_method_to_string(endpoint->method), 
                         endpoint->path, 
                         endpoint->handler);
            }
        }
    }
    
    ts_query_cursor_delete(cursor);
}

static void extract_calls(TSTree* tree, const char* source, ParseResult* result, const char* service_name) {
    if (!python_state.calls_query) return;
    
    TSNode root_node = ts_tree_root_node(tree);
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, python_state.calls_query, root_node);
    
    TSQueryMatch match;
    char lib_buffer[128];
    char method_buffer[128];
    char url_buffer[512];
    
    while (ts_query_cursor_next_match(cursor, &match)) {
        const char* lib_name = NULL;
        const char* method_name = NULL;
        const char* url = NULL;
        
        for (uint16_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            uint32_t capture_name_len;
            const char* capture_name = ts_query_capture_name_for_id(
                python_state.calls_query, capture.index, &capture_name_len);
            
            TSNode node = capture.node;
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            uint32_t length = end - start;
            
            if (strncmp(capture_name, "http.client.lib", capture_name_len) == 0) {
                if (length < sizeof(lib_buffer)) {
                    memcpy(lib_buffer, source + start, length);
                    lib_buffer[length] = '\0';
                    lib_name = lib_buffer;
                }
            } else if (strncmp(capture_name, "http.client.method", capture_name_len) == 0) {
                if (length < sizeof(method_buffer)) {
                    memcpy(method_buffer, source + start, length);
                    method_buffer[length] = '\0';
                    method_name = method_buffer;
                }
            } else if (strncmp(capture_name, "http.client.url", capture_name_len) == 0) {
                if (length < sizeof(url_buffer) - 1) {
                    memcpy(url_buffer, source + start, length);
                    url_buffer[length] = '\0';
                    // Strip quotes
                    if (url_buffer[0] == '"' || url_buffer[0] == '\'') {
                        url = url_buffer + 1;
                        if (length > 2) url_buffer[length - 1] = '\0';
                    } else {
                        url = url_buffer;
                    }
                }
            }
        }
        
        // Create edge if this looks like an HTTP call (e.g., requests.get)
        if (lib_name && method_name && url && service_name) {
            // Check if it's a known HTTP library
            if (strcmp(lib_name, "requests") == 0 || strcmp(lib_name, "httpx") == 0) {
                Edge* edge = edge_create(
                    service_name,
                    url,  // Use URL as target service (simplified)
                    EDGE_HTTP_CALL,
                    method_name,
                    url,
                    path_basename(result->service->path),
                    ts_node_start_point(match.captures[0].node).row + 1
                );
                
                if (edge) {
                    edge_set_confidence(edge, 0.8f);  // Medium confidence for inferred calls
                    edge_list_add(result->edges, edge);
                    LOG_DEBUG("Found HTTP call: %s.%s(%s)", lib_name, method_name, url);
                }
            }
        }
    }
    
    ts_query_cursor_delete(cursor);
}

static void extract_imports(TSTree* tree, const char* source, ParseResult* result) {
    if (!python_state.functions_query) return;  // Reusing functions_query for imports
    
    TSNode root_node = ts_tree_root_node(tree);
    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, python_state.functions_query, root_node);
    
    TSQueryMatch match;
    char import_buffer[256];
    
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            TSNode node = capture.node;
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            uint32_t length = end - start;
            
            if (length < sizeof(import_buffer)) {
                memcpy(import_buffer, source + start, length);
                import_buffer[length] = '\0';
                parse_result_add_import(result, import_buffer);
                LOG_DEBUG("Found import: %s", import_buffer);
            }
        }
    }
    
    ts_query_cursor_delete(cursor);
}