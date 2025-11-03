#include "../plugin.h"
#include "../../util/logger.h"
#include "../../util/path.h"
#include <stdlib.h>
#include <string.h>

/* Python plugin state */
static struct {
    bool initialized;
} python_state = {0};

/* Forward declarations */
static bool python_init(void);
static void python_shutdown(void);
static bool python_supports_file(const char* filepath);
static ParseResult* python_parse_file(const char* filepath, const char* service_name);
static const char* python_get_query_path(const char* query_name);
static char* python_infer_service_name(const char* filepath);

/* Supported file extensions */
static const char* python_extensions[] = {
    "py",
    "pyi",  // Python stub files
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
    
    // TODO: Initialize Tree-sitter Python grammar
    // TODO: Load query files from src/lang/python/queries/
    
    python_state.initialized = true;
    LOG_INFO("Python plugin initialized successfully");
    
    return true;
}

static void python_shutdown(void) {
    if (!python_state.initialized) {
        return;
    }
    
    LOG_DEBUG("Shutting down Python plugin...");
    
    // TODO: Cleanup Tree-sitter resources
    
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
    
    LOG_DEBUG("Parsing Python file: %s", filepath);
    
    ParseResult* result = parse_result_create();
    if (!result) return NULL;
    
    // TODO: Implement actual Tree-sitter parsing
    // For now, just return a stub result
    
    result->success = true;
    
    // Create a dummy service
    if (service_name) {
        result->service = service_create(service_name, "python", filepath);
    }
    
    LOG_DEBUG("Python parsing complete (stub implementation)");
    
    return result;
}

static const char* python_get_query_path(const char* query_name) {
    // TODO: Return path to actual .scm query files
    (void)query_name;
    return NULL;
}

static char* python_infer_service_name(const char* filepath) {
    if (!filepath) return NULL;
    
    // Simple heuristic: use parent directory name
    char* dir = path_dirname(filepath);
    if (!dir) return NULL;
    
    const char* basename = path_basename(dir);
    char* service_name = strdup(basename);
    
    free(dir);
    return service_name;
}