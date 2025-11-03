#include <stdio.h>
#include <stdlib.h>
#include "core/entity.h"
#include "core/walker.h"
#include "lang/plugin.h"
#include "util/logger.h"

/* Global collections for scan results */
static ServiceList* g_services = NULL;
static EndpointList* g_endpoints = NULL;
static EdgeList* g_edges = NULL;

/* Test Section 1: Entity System */
static void test_entity_system(void) {
    log_info("========================================");
    log_info("Testing Entity System");
    log_info("========================================");
    
    // Create a service
    Service* service = service_create("auth-service", "python", "./services/auth");
    if (service) {
        log_info("✓ Created service: %s (%s)", service->name, service->language);
        service_add_file(service, "services/auth/main.py");
        service_add_file(service, "services/auth/routes.py");
        LOG_DEBUG("Added %zu files to service", service->file_count);
        service_free(service);
    } else {
        log_error("✗ Failed to create service");
    }
    
    // Create an endpoint
    Endpoint* endpoint = endpoint_create(
        "auth-service", 
        "/api/login", 
        HTTP_POST, 
        "login_handler", 
        "routes.py", 
        42
    );
    if (endpoint) {
        log_info("✓ Created endpoint: %s %s", 
                 http_method_to_string(endpoint->method), 
                 endpoint->path);
        endpoint_free(endpoint);
    } else {
        log_error("✗ Failed to create endpoint");
    }
    
    // Create an edge
    Edge* edge = edge_create(
        "auth-service", 
        "user-db", 
        EDGE_DATABASE, 
        NULL, 
        NULL, 
        "main.py", 
        15
    );
    if (edge) {
        log_info("✓ Created edge: %s -> %s (%s)", 
                 edge->from_service, 
                 edge->to_service, 
                 edge_type_to_string(edge->type));
        edge_set_confidence(edge, 0.95f);
        LOG_DEBUG("Edge confidence: %.2f", edge->confidence);
        edge_free(edge);
    } else {
        log_error("✗ Failed to create edge");
    }
    
    // Test collections
    ServiceList* services = service_list_create();
    if (services) {
        Service* s1 = service_create("api", "python", "./api");
        Service* s2 = service_create("worker", "python", "./worker");
        
        service_list_add(services, s1);
        service_list_add(services, s2);
        
        log_info("✓ Created service list with %zu services", services->count);
        
        Service* found = service_list_find(services, "api");
        if (found) {
            LOG_DEBUG("Successfully found service: %s", found->name);
        }
        
        service_list_free(services);
    }
    
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
    
    // Configure walker to find Python files
    WalkerConfig config = walker_config_default();
    const char* python_exts[] = {"py"};
    config.extensions = python_exts;
    config.extension_count = 1;
    config.max_depth = 5;
    
    // Walk the directory
    log_info("Walking directory: %s", root_path);
    g_files_found = 0;
    
    bool success = walker_walk(root_path, &config, file_found_callback, NULL);
    
    if (success) {
        log_info("✓ Walker completed successfully");
        
        WalkerStats stats = walker_get_stats();
        log_info("Statistics:");
        log_info("  Directories visited: %zu", stats.directories_visited);
        log_info("  Files scanned: %zu", stats.files_scanned);
        log_info("  Python files found: %zu", stats.files_matched);
        log_info("  Files ignored: %zu", stats.files_ignored);
        
        if (stats.errors > 0) {
            log_warn("  Errors encountered: %zu", stats.errors);
        }
    } else {
        log_error("✗ Walker failed");
    }
    
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
    
    log_info("Plugin system tests complete!\n");
}

/* Test Section 4: Full Integration - Parse Real Files */
static void parse_file_callback(const char* filepath, void* userdata) {
    (void)userdata;
    
    // Get appropriate plugin
    LanguagePlugin* plugin = plugin_registry_get_for_file(filepath);
    if (!plugin) {
        LOG_DEBUG("No plugin for file: %s", filepath);
        return;
    }
    
    // Infer service name from path
    char* service_name = plugin->infer_service_name(filepath);
    
    // Parse the file
    ParseResult* result = plugin->parse_file(filepath, service_name);
    free(service_name);
    
    if (!result) {
        LOG_WARN("Failed to parse: %s", filepath);
        return;
    }
    
    if (!result->success) {
        LOG_WARN("Parse error in %s: %s", filepath, 
                 result->error_message ? result->error_message : "unknown");
        parse_result_free(result);
        return;
    }
    
    // Collect results
    if (result->service) {
        // Check if service already exists
        Service* existing = service_list_find(g_services, result->service->name);
        if (existing) {
            // Add file to existing service
            service_add_file(existing, filepath);
            service_free(result->service);
            result->service = NULL;
        } else {
            // Add new service
            service_list_add(g_services, result->service);
            result->service = NULL;  // Transfer ownership
        }
    }
    
    // Collect endpoints
    for (size_t i = 0; i < result->endpoints->count; i++) {
        Endpoint* endpoint = result->endpoints->items[i];
        result->endpoints->items[i] = NULL;  // Transfer ownership
        endpoint_list_add(g_endpoints, endpoint);
    }
    
    // Collect edges
    for (size_t i = 0; i < result->edges->count; i++) {
        Edge* edge = result->edges->items[i];
        result->edges->items[i] = NULL;  // Transfer ownership
        edge_list_add(g_edges, edge);
    }
    
    LOG_DEBUG("Parsed %s: %zu endpoints, %zu edges, %zu imports",
              filepath, result->endpoints->count, result->edges->count, result->import_count);
    
    parse_result_free(result);
}

static void test_full_scan(const char* root_path) {
    log_info("========================================");
    log_info("Full Repository Scan");
    log_info("========================================");
    
    // Initialize collections
    g_services = service_list_create();
    g_endpoints = endpoint_list_create();
    g_edges = edge_list_create();
    
    if (!g_services || !g_endpoints || !g_edges) {
        log_error("Failed to create result collections");
        return;
    }
    
    // Configure walker
    WalkerConfig config = walker_config_default();
    const char* python_exts[] = {"py"};
    config.extensions = python_exts;
    config.extension_count = 1;
    config.max_depth = 10;
    
    log_info("Scanning repository: %s", root_path);
    
    // Walk and parse all Python files
    bool success = walker_walk(root_path, &config, parse_file_callback, NULL);
    
    if (!success) {
        log_error("✗ Scan failed");
        return;
    }
    
    // Display results
    log_info("\n========================================");
    log_info("Scan Results");
    log_info("========================================");
    
    log_info("Services discovered: %zu", g_services->count);
    for (size_t i = 0; i < g_services->count; i++) {
        Service* svc = g_services->items[i];
        log_info("  [%s] %s (%s) - %zu files", 
                 svc->language, svc->name, svc->path, svc->file_count);
    }
    
    log_info("\nEndpoints discovered: %zu", g_endpoints->count);
    for (size_t i = 0; i < g_endpoints->count; i++) {
        Endpoint* ep = g_endpoints->items[i];
        log_info("  %s %s -> %s() in %s:%d",
                 http_method_to_string(ep->method),
                 ep->path,
                 ep->handler ? ep->handler : "?",
                 ep->file ? ep->file : "?",
                 ep->line);
    }
    
    log_info("\nDependencies discovered: %zu", g_edges->count);
    for (size_t i = 0; i < g_edges->count; i++) {
        Edge* edge = g_edges->items[i];
        log_info("  %s -> %s (%s) [confidence: %.2f]",
                 edge->from_service,
                 edge->to_service,
                 edge_type_to_string(edge->type),
                 edge->confidence);
    }
    
    WalkerStats stats = walker_get_stats();
    log_info("\nScan Statistics:");
    log_info("  Files analyzed: %zu", stats.files_matched);
    log_info("  Services: %zu", g_services->count);
    log_info("  Endpoints: %zu", g_endpoints->count);
    log_info("  Dependencies: %zu", g_edges->count);
    
    // Cleanup
    service_list_free(g_services);
    endpoint_list_free(g_endpoints);
    edge_list_free(g_edges);
    
    log_info("\nFull scan complete!\n");
}

int main(int argc, char** argv) {
    // Initialize logger
    logger_init(LOG_LEVEL_INFO, LOG_OUTPUT_STDOUT, NULL);
    
    log_info("========================================");
    log_info("Brightpanda v1.0.0");
    log_info("Code Architecture Analyzer");
    log_info("========================================\n");
    
    if (argc < 2) {
        log_error("Usage: %s <directory>", argv[0]);
        log_info("Example: %s /path/to/python/project", argv[0]);
        logger_shutdown();
        return 1;
    }
    
    const char* root_path = argv[1];
    
    // Run all tests in sequence
    test_entity_system();
    test_walker_system(root_path);
    test_plugin_system();
    test_full_scan(root_path);
    
    // Summary
    log_info("========================================");
    log_info("All systems operational!");
    log_info("========================================");
    
    // Cleanup
    plugin_registry_shutdown();
    logger_shutdown();
    return 0;
}