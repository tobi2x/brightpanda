#include "../plugin.h"
#include "../../core/parser_pool.h"
#include "../../core/extractor.h"
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
    TSQuery* routes_query;
    TSQuery* calls_query;
    TSQuery* imports_query;
    char* query_dir;
} python_state = {0};

/* Context for route extraction */
typedef struct {
    ParseResult* result;
    const char* service_name;
} RouteContext;

/* Context for call extraction */
typedef struct {
    ParseResult* result;
    const char* service_name;
} CallContext;

/* Context for import extraction */
typedef struct {
    ParseResult* result;
} ImportContext;

/* Forward declarations */
static bool python_init(void);
static void python_shutdown(void);
static bool python_supports_file(const char* filepath);
static ParseResult* python_parse_file(const char* filepath, const char* service_name);
static const char* python_get_query_path(const char* query_name);
static char* python_infer_service_name(const char* filepath);

/* Helper functions */
static char* read_file_contents(const char* filepath);
static char* get_query_file_path(const char* query_name);
static TSQuery* load_query_from_file(const TSLanguage* language, const char* query_file);
static TSQuery* create_fallback_query(const TSLanguage* language, const char* query_name);

/* Extraction callbacks */
static void extract_route_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata);
static void extract_call_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata);
static void extract_import_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata);

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
    
    // Set query directory (relative to executable or default)
    python_state.query_dir = strdup("../src/lang/python/queries");
    LOG_DEBUG("Query directory: %s", python_state.query_dir);
    
    // Try to load query files
    python_state.routes_query = load_query_from_file(language, "routes.scm");
    python_state.calls_query = load_query_from_file(language, "calls.scm");
    python_state.imports_query = load_query_from_file(language, "imports.scm");
    
    // Fall back to inline queries if files don't exist
    if (!python_state.routes_query) {
        LOG_WARN("routes.scm not found, using fallback query");
        python_state.routes_query = create_fallback_query(language, "routes");
    }
    
    if (!python_state.calls_query) {
        LOG_WARN("calls.scm not found, using fallback query");
        python_state.calls_query = create_fallback_query(language, "calls");
    }
    
    if (!python_state.imports_query) {
        LOG_WARN("imports.scm not found, using fallback query");
        python_state.imports_query = create_fallback_query(language, "imports");
    }
    
    if (!python_state.routes_query || !python_state.calls_query || !python_state.imports_query) {
        LOG_ERROR("Failed to load or create queries");
        python_shutdown();
        return false;
    }
    
    python_state.initialized = true;
    LOG_INFO("Python plugin initialized successfully");
    
    return true;
}

static void python_shutdown(void) {
    if (!python_state.initialized) {
        return;
    }
    
    LOG_DEBUG("Shutting down Python plugin...");
    
    if (python_state.routes_query) {
        ts_query_delete(python_state.routes_query);
        python_state.routes_query = NULL;
    }
    
    if (python_state.calls_query) {
        ts_query_delete(python_state.calls_query);
        python_state.calls_query = NULL;
    }
    
    if (python_state.imports_query) {
        ts_query_delete(python_state.imports_query);
        python_state.imports_query = NULL;
    }
    
    free(python_state.query_dir);
    python_state.query_dir = NULL;
    
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
    }
    
    // Infer service name if not provided
    if (!service_name) {
        service_name = python_infer_service_name(filepath);
    }
    
    result->service = service_create(service_name ? service_name : "unknown", "python", filepath);
    
    // Extract routes using the extractor
    RouteContext route_ctx = { .result = result, .service_name = service_name };
    extractor_execute_query(python_state.routes_query, tree, source, extract_route_match, &route_ctx);
    
    // Extract calls using the extractor
    CallContext call_ctx = { .result = result, .service_name = service_name };
    extractor_execute_query(python_state.calls_query, tree, source, extract_call_match, &call_ctx);
    
    // Extract imports using the extractor
    ImportContext import_ctx = { .result = result };
    extractor_execute_query(python_state.imports_query, tree, source, extract_import_match, &import_ctx);
    
    result->success = true;
    
    // Cleanup
    ts_tree_delete(tree);
    parser_pool_release(parser);
    free(source);
    
    LOG_DEBUG("Python parsing complete: %zu endpoints, %zu edges, %zu imports",
              result->endpoints->count, result->edges->count, result->import_count);
    
    return result;
}

static void extract_route_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata) {
    RouteContext* ctx = (RouteContext*)userdata;
    
    TSNode route_path_node, handler_node;
    
    // Find captures by name
    bool has_path = extractor_find_capture(match, query, "route.path", &route_path_node) ||
                    extractor_find_capture(match, query, "fastapi.path", &route_path_node);
    
    bool has_handler = extractor_find_capture(match, query, "route.handler", &handler_node) ||
                       extractor_find_capture(match, query, "fastapi.handler", &handler_node);
    
    if (!has_path || !has_handler) return;
    
    // Get path and handler text
    char* path = extractor_get_node_text(route_path_node, source);
    char* handler = extractor_get_node_text(handler_node, source);
    
    if (!path || !handler) {
        free(path);
        free(handler);
        return;
    }
    
    // Strip quotes from path
    char* clean_path = extractor_strip_quotes(path);
    free(path);
    
    // Create endpoint
    Endpoint* endpoint = endpoint_create(
        ctx->service_name ? ctx->service_name : "unknown",
        clean_path,
        HTTP_GET,  // Default, would need more logic to detect actual method
        handler,
        path_basename(ctx->result->service->path),
        ts_node_start_point(handler_node).row + 1
    );
    
    if (endpoint) {
        endpoint_list_add(ctx->result->endpoints, endpoint);
        LOG_DEBUG("Found endpoint: %s %s -> %s()", 
                 http_method_to_string(endpoint->method), 
                 endpoint->path, 
                 endpoint->handler);
    }
    
    free(clean_path);
    free(handler);
}

static void extract_call_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata) {
    CallContext* ctx = (CallContext*)userdata;
    
    TSNode lib_node, method_node, url_node;
    
    // Find HTTP client calls
    bool has_lib = extractor_find_capture(match, query, "http.client.lib", &lib_node);
    bool has_method = extractor_find_capture(match, query, "http.client.method", &method_node);
    bool has_url = extractor_find_capture(match, query, "http.client.url", &url_node);
    
    if (!has_lib || !has_method || !has_url) return;
    
    // Get text
    char* lib = extractor_get_node_text(lib_node, source);
    char* method = extractor_get_node_text(method_node, source);
    char* url = extractor_get_node_text(url_node, source);
    
    if (!lib || !method || !url) {
        free(lib);
        free(method);
        free(url);
        return;
    }
    
    // Check if it's a known HTTP library
    if (strcmp(lib, "requests") == 0 || strcmp(lib, "httpx") == 0) {
        char* clean_url = extractor_strip_quotes(url);
        
        Edge* edge = edge_create(
            ctx->service_name ? ctx->service_name : "unknown",
            clean_url,
            EDGE_HTTP_CALL,
            method,
            clean_url,
            path_basename(ctx->result->service->path),
            ts_node_start_point(lib_node).row + 1
        );
        
        if (edge) {
            edge_set_confidence(edge, 0.8f);
            edge_list_add(ctx->result->edges, edge);
            LOG_DEBUG("Found HTTP call: %s.%s(%s)", lib, method, clean_url);
        }
        
        free(clean_url);
    }
    
    free(lib);
    free(method);
    free(url);
}

static void extract_import_match(TSQueryMatch match, TSQuery* query, const char* source, void* userdata) {
    ImportContext* ctx = (ImportContext*)userdata;
    
    TSNode import_node;
    
    // Find import module
    bool has_import = extractor_find_capture(match, query, "import.module", &import_node) ||
                      extractor_find_capture(match, query, "import.from.module", &import_node);
    
    if (!has_import) return;
    
    char* module = extractor_get_node_text(import_node, source);
    if (module) {
        parse_result_add_import(ctx->result, module);
        LOG_DEBUG("Found import: %s", module);
        free(module);
    }
}

static const char* python_get_query_path(const char* query_name) {
    return get_query_file_path(query_name);
}

static char* python_infer_service_name(const char* filepath) {
    if (!filepath) return NULL;
    
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
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0 || size > 10 * 1024 * 1024) {
        LOG_ERROR("File too large or invalid: %s", filepath);
        fclose(file);
        return NULL;
    }
    
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

static char* get_query_file_path(const char* query_name) {
    if (!python_state.query_dir || !query_name) {
        return NULL;
    }
    
    return path_join(python_state.query_dir, query_name);
}

static TSQuery* load_query_from_file(const TSLanguage* language, const char* query_file) {
    char* full_path = get_query_file_path(query_file);
    if (!full_path) {
        return NULL;
    }
    
    LOG_DEBUG("Loading query from: %s", full_path);
    
    if (!path_exists(full_path)) {
        LOG_DEBUG("Query file not found: %s", full_path);
        free(full_path);
        return NULL;
    }
    
    char* query_string = read_file_contents(full_path);
    free(full_path);
    
    if (!query_string) {
        return NULL;
    }
    
    uint32_t error_offset;
    TSQueryError error_type;
    
    TSQuery* query = ts_query_new(language, query_string, strlen(query_string),
                                  &error_offset, &error_type);
    
    if (!query) {
        LOG_ERROR("Failed to parse query file %s at offset %u: error type %d",
                 query_file, error_offset, error_type);
        free(query_string);
        return NULL;
    }
    
    free(query_string);
    LOG_INFO("Successfully loaded query: %s", query_file);
    
    return query;
}

static TSQuery* create_fallback_query(const TSLanguage* language, const char* query_name) {
    const char* query_string = NULL;
    
    if (strcmp(query_name, "routes") == 0) {
        query_string = 
            "(decorated_definition\n"
            "  (decorator\n"
            "    (call\n"
            "      function: (attribute\n"
            "        attribute: (identifier) @route.decorator)\n"
            "      arguments: (argument_list\n"
            "        (string) @route.path)))\n"
            "  definition: (function_definition\n"
            "    name: (identifier) @route.handler))\n";
    } else if (strcmp(query_name, "calls") == 0) {
        query_string =
            "(call\n"
            "  function: (attribute\n"
            "    object: (identifier) @http.client.lib\n"
            "    attribute: (identifier) @http.client.method)\n"
            "  arguments: (argument_list\n"
            "    (string) @http.client.url))\n";
    } else if (strcmp(query_name, "imports") == 0) {
        query_string =
            "(import_statement\n"
            "  name: (dotted_name) @import.module)\n"
            "(import_from_statement\n"
            "  module_name: (dotted_name) @import.from.module)\n";
    }
    
    if (!query_string) return NULL;
    
    uint32_t error_offset;
    TSQueryError error_type;
    
    return ts_query_new(language, query_string, strlen(query_string),
                       &error_offset, &error_type);
}