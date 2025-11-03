#ifndef BRIGHTPANDA_PLUGIN_H
#define BRIGHTPANDA_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>
#include "../core/entity.h"

/*
 * Language plugin interface for Brightpanda.
 * Each language (Python, JavaScript, Go, etc.) implements this interface.
 */

/* Forward declarations */
typedef struct LanguagePlugin LanguagePlugin;
typedef struct ParseResult ParseResult;

/* Parse result contains extracted entities from a source file */
struct ParseResult {
    Service* service;           // Service information (if detected)
    EndpointList* endpoints;    // Endpoints found in this file
    EdgeList* edges;            // Dependencies/calls found in this file
    char** imports;             // Import statements
    size_t import_count;
    bool success;               // Whether parsing succeeded
    char* error_message;        // Error message if parsing failed
};

/* Create a new parse result */
ParseResult* parse_result_create(void);

/* Free a parse result */
void parse_result_free(ParseResult* result);

/* Add an import to the parse result */
bool parse_result_add_import(ParseResult* result, const char* import);

/* Plugin interface - each language must implement these functions */
struct LanguagePlugin {
    /* Plugin metadata */
    const char* name;               // Language name (e.g., "python")
    const char* version;            // Plugin version
    const char** file_extensions;   // Supported extensions (NULL-terminated)
    
    /* Initialization */
    bool (*init)(void);             // Initialize plugin (load grammars, etc.)
    void (*shutdown)(void);         // Cleanup plugin resources
    
    /* Core functionality */
    bool (*supports_file)(const char* filepath);  // Check if plugin can handle file
    ParseResult* (*parse_file)(const char* filepath, const char* service_name);
    
    /* Query management (Tree-sitter specific) */
    const char* (*get_query_path)(const char* query_name);  // Get path to .scm file
    
    /* Optional: language-specific heuristics */
    char* (*infer_service_name)(const char* filepath);  // Guess service from path
};

/* Plugin registry functions */

/* Register a language plugin */
bool plugin_registry_register(LanguagePlugin* plugin);

/* Get a plugin by language name */
LanguagePlugin* plugin_registry_get(const char* language);

/* Get a plugin that supports a given file */
LanguagePlugin* plugin_registry_get_for_file(const char* filepath);

/* List all registered plugins */
LanguagePlugin** plugin_registry_list(size_t* count);

/* Initialize the plugin registry */
bool plugin_registry_init(void);

/* Shutdown and cleanup all plugins */
void plugin_registry_shutdown(void);

#endif // BRIGHTPANDA_PLUGIN_H