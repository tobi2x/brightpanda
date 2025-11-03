#include "plugin.h"
#include "../util/logger.h"
#include "../util/path.h"
#include <stdlib.h>
#include <string.h>

#define MAX_PLUGINS 16

/* Global plugin registry */
static struct {
    LanguagePlugin* plugins[MAX_PLUGINS];
    size_t count;
    bool initialized;
} g_registry = {0};

/* External plugin declarations - these will be implemented in language-specific files */
extern LanguagePlugin* python_plugin_create(void);

bool plugin_registry_init(void) {
    if (g_registry.initialized) {
        return true;
    }
    
    LOG_INFO("Initializing plugin registry...");
    
    // Register Python plugin
    LanguagePlugin* python = python_plugin_create();
    if (python) {
        if (plugin_registry_register(python)) {
            LOG_INFO("Registered plugin: %s v%s", python->name, python->version);
        } else {
            LOG_ERROR("Failed to register Python plugin");
            return false;
        }
    } else {
        LOG_ERROR("Failed to create Python plugin");
        return false;
    }
    
    // Future: Register other language plugins here
    // LanguagePlugin* javascript = javascript_plugin_create();
    // plugin_registry_register(javascript);
    
    g_registry.initialized = true;
    LOG_INFO("Plugin registry initialized with %zu plugins", g_registry.count);
    
    return true;
}

bool plugin_registry_register(LanguagePlugin* plugin) {
    if (!plugin) {
        LOG_ERROR("Cannot register NULL plugin");
        return false;
    }
    
    if (g_registry.count >= MAX_PLUGINS) {
        LOG_ERROR("Plugin registry full (max %d plugins)", MAX_PLUGINS);
        return false;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.plugins[i]->name, plugin->name) == 0) {
            LOG_WARN("Plugin '%s' already registered", plugin->name);
            return false;
        }
    }
    
    // Initialize the plugin
    if (plugin->init && !plugin->init()) {
        LOG_ERROR("Failed to initialize plugin: %s", plugin->name);
        return false;
    }
    
    g_registry.plugins[g_registry.count++] = plugin;
    LOG_DEBUG("Plugin '%s' registered successfully", plugin->name);
    
    return true;
}

LanguagePlugin* plugin_registry_get(const char* language) {
    if (!language) return NULL;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcasecmp(g_registry.plugins[i]->name, language) == 0) {
            return g_registry.plugins[i];
        }
    }
    
    return NULL;
}

LanguagePlugin* plugin_registry_get_for_file(const char* filepath) {
    if (!filepath) return NULL;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        LanguagePlugin* plugin = g_registry.plugins[i];
        if (plugin->supports_file && plugin->supports_file(filepath)) {
            return plugin;
        }
    }
    
    return NULL;
}

LanguagePlugin** plugin_registry_list(size_t* count) {
    if (count) {
        *count = g_registry.count;
    }
    return g_registry.plugins;
}

void plugin_registry_shutdown(void) {
    if (!g_registry.initialized) {
        return;
    }
    
    LOG_INFO("Shutting down plugin registry...");
    
    for (size_t i = 0; i < g_registry.count; i++) {
        LanguagePlugin* plugin = g_registry.plugins[i];
        if (plugin->shutdown) {
            LOG_DEBUG("Shutting down plugin: %s", plugin->name);
            plugin->shutdown();
        }
        // Note: We don't free the plugin itself as it may be statically allocated
    }
    
    g_registry.count = 0;
    g_registry.initialized = false;
    
    LOG_INFO("Plugin registry shutdown complete");
}

/* ParseResult implementation */

ParseResult* parse_result_create(void) {
    ParseResult* result = calloc(1, sizeof(ParseResult));
    if (!result) return NULL;
    
    result->endpoints = endpoint_list_create();
    result->edges = edge_list_create();
    result->success = false;
    
    if (!result->endpoints || !result->edges) {
        parse_result_free(result);
        return NULL;
    }
    
    return result;
}

void parse_result_free(ParseResult* result) {
    if (!result) return;
    
    if (result->service) {
        service_free(result->service);
    }
    
    if (result->endpoints) {
        endpoint_list_free(result->endpoints);
    }
    
    if (result->edges) {
        edge_list_free(result->edges);
    }
    
    if (result->imports) {
        for (size_t i = 0; i < result->import_count; i++) {
            free(result->imports[i]);
        }
        free(result->imports);
    }
    
    free(result->error_message);
    free(result);
}

bool parse_result_add_import(ParseResult* result, const char* import) {
    if (!result || !import) return false;
    
    // Resize array if needed
    size_t new_size = result->import_count + 1;
    char** new_imports = realloc(result->imports, new_size * sizeof(char*));
    if (!new_imports) return false;
    
    result->imports = new_imports;
    result->imports[result->import_count] = strdup(import);
    if (!result->imports[result->import_count]) return false;
    
    result->import_count++;
    return true;
}