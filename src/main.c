#include <stdio.h>
#include <stdlib.h>
#include "core/entity.h"
#include "core/walker.h"
#include "lang/plugin.h"
#include "util/logger.h"

/* Test Section 1: Entity System */
static void test_entity_system(void) {
    log_info("========================================");
    log_info("Testing Entity System");
    log_info("========================================");
    
    // ... (keep existing entity tests) ...
    
    log_info("Entity system tests complete!\n");
}

/* Test Section 2: Walker System */
static size_t g_files_found = 0;

static void file_found_callback(const char* filepath, void* userdata) {
    (void)userdata;
    g_files_found++;
    LOG_DEBUG("  Found: %s", filepath);
}

static void test_walker_system(const char* root_path) {
    log_info("========================================");
    log_info("Testing Walker System");
    log_info("========================================");
    
    // ... (keep existing walker tests) ...
    
    log_info("Walker tests complete!\n");
}

/* Test Section 3: Plugin System */
static void test_plugin_system(void) {
    log_info("========================================");
    log_info("Testing Plugin System");
    log_info("========================================");
    
    // Initialize plugin registry
    if (!plugin_registry_init()) {
        log_error("✗ Failed to initialize plugin registry");
        return;
    }
    log_info("✓ Plugin registry initialized");
    
    // List registered plugins
    size_t count;
    LanguagePlugin** plugins = plugin_registry_list(&count);
    log_info("Registered plugins: %zu", count);
    
    for (size_t i = 0; i < count; i++) {
        log_info("  - %s v%s", plugins[i]->name, plugins[i]->version);
        
        // Show supported extensions
        LOG_DEBUG("    Extensions:");
        for (size_t j = 0; plugins[i]->file_extensions[j]; j++) {
            LOG_DEBUG("      .%s", plugins[i]->file_extensions[j]);
        }
    }
    
    // Test file detection
    const char* test_files[] = {
        "test.py",
        "main.js",
        "app.go",
        NULL
    };
    
    for (size_t i = 0; test_files[i]; i++) {
        LanguagePlugin* plugin = plugin_registry_get_for_file(test_files[i]);
        if (plugin) {
            log_info("✓ %s -> %s plugin", test_files[i], plugin->name);
        } else {
            LOG_DEBUG("  %s -> no plugin found", test_files[i]);
        }
    }
    
    // Test parsing (stub for now)
    LanguagePlugin* python = plugin_registry_get("python");
    if (python) {
        ParseResult* result = python->parse_file("example.py", "test-service");
        if (result) {
            if (result->success) {
                log_info("✓ Successfully parsed Python file (stub)");
            }
            parse_result_free(result);
        }
    }
    
    log_info("Plugin system tests complete!\n");
}

int main(int argc, char** argv) {
    // Initialize logger
    logger_init(LOG_LEVEL_INFO, LOG_OUTPUT_STDOUT, NULL);
    
    log_info("========================================");
    log_info("Brightpanda v1.0.0");
    log_info("========================================\n");
    
    if (argc < 2) {
        log_error("Usage: %s <directory>", argv[0]);
        log_info("Example: %s /path/to/project", argv[0]);
        logger_shutdown();
        return 1;
    }
    
    const char* root_path = argv[1];
    
    // Run all tests in sequence
    test_entity_system();
    test_walker_system(root_path);
    test_plugin_system();
    
    // Summary
    log_info("========================================");
    log_info("All tests complete!");
    log_info("========================================");
    
    // Cleanup
    plugin_registry_shutdown();
    logger_shutdown();
    return 0;
}