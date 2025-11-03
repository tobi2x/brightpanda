#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "core/entity.h"
#include "core/walker.h"
#include "core/manifest.h"
#include "lang/plugin.h"
#include "util/logger.h"
#include "util/path.h"

/* Test Section 1: Entity System */
static void test_entity_system(void) {
    log_info("========================================");
    log_info("Testing Entity System");
    log_info("========================================");
    
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
    
    Endpoint* endpoint = endpoint_create("auth-service", "/api/login", HTTP_POST, 
                                        "login_handler", "routes.py", 42);
    if (endpoint) {
        log_info("✓ Created endpoint: %s %s", 
                 http_method_to_string(endpoint->method), endpoint->path);
        endpoint_free(endpoint);
    } else {
        log_error("✗ Failed to create endpoint");
    }
    
    Edge* edge = edge_create("auth-service", "user-db", EDGE_DATABASE, 
                            NULL, NULL, "main.py", 15);
    if (edge) {
        log_info("✓ Created edge: %s -> %s (%s)", 
                 edge->from_service, edge->to_service, edge_type_to_string(edge->type));
        edge_free(edge);
    } else {
        log_error("✗ Failed to create edge");
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
    
    WalkerConfig config = walker_config_default();
    const char* python_exts[] = {"py"};
    config.extensions = python_exts;
    config.extension_count = 1;
    config.max_depth = 5;
    
    log_info("Walking directory: %s", root_path);
    g_files_found = 0;
    
    bool success = walker_walk(root_path, &config, file_found_callback, NULL);
    
    if (success) {
        log_info("✓ Walker completed successfully");
        WalkerStats stats = walker_get_stats();
        log_info("  Python files found: %zu", stats.files_matched);
    }
    
    log_info("Walker tests complete!\n");
}

/* Test Section 3: Plugin System */
static void test_plugin_system(void) {
    log_info("========================================");
    log_info("Testing Plugin System");
    log_info("========================================");
    
    if (!plugin_registry_init()) {
        log_error("✗ Failed to initialize plugin registry");
        return;
    }
    log_info("✓ Plugin registry initialized");
    
    size_t count;
    LanguagePlugin** plugins = plugin_registry_list(&count);
    log_info("Registered plugins: %zu", count);
    
    for (size_t i = 0; i < count; i++) {
        log_info("  - %s v%s", plugins[i]->name, plugins[i]->version);
    }
    
    log_info("Plugin system tests complete!\n");
}

/* Test Section 4: Full Integration with Manifest */
typedef struct {
    Manifest* manifest;
    size_t files_processed;
    size_t files_with_endpoints;
    size_t files_with_edges;
} ScanContext;

static void parse_and_collect_callback(const char* filepath, void* userdata) {
    ScanContext* ctx = (ScanContext*)userdata;
    
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
    
    ctx->files_processed++;
    
    // Add service to manifest
    if (result->service) {
        Service* existing = service_list_find(ctx->manifest->services, result->service->name);
        if (existing) {
            // Add file to existing service
            service_add_file(existing, filepath);
            service_free(result->service);
            result->service = NULL;
        } else {
            // Add new service (transfer ownership)
            Service* service = result->service;
            result->service = NULL;
            manifest_add_service(ctx->manifest, service);
        }
    }
    
    // Add endpoints to manifest
    if (result->endpoints->count > 0) {
        ctx->files_with_endpoints++;
        for (size_t i = 0; i < result->endpoints->count; i++) {
            Endpoint* endpoint = result->endpoints->items[i];
            result->endpoints->items[i] = NULL;  // Transfer ownership
            manifest_add_endpoint(ctx->manifest, endpoint);
        }
    }
    
    // Add edges to manifest
    if (result->edges->count > 0) {
        ctx->files_with_edges++;
        for (size_t i = 0; i < result->edges->count; i++) {
            Edge* edge = result->edges->items[i];
            result->edges->items[i] = NULL;  // Transfer ownership
            manifest_add_edge(ctx->manifest, edge);
        }
    }
    
    LOG_DEBUG("Parsed %s: %zu endpoints, %zu edges, %zu imports",
              filepath, result->endpoints->count, result->edges->count, result->import_count);
    
    parse_result_free(result);
}

static void test_full_scan(const char* root_path, const char* output_file) {
    log_info("========================================");
    log_info("Full Repository Scan");
    log_info("========================================");
    
    // Get repo name from path
    const char* repo_name = path_basename(root_path);
    
    // Create manifest
    Manifest* manifest = manifest_create(repo_name);
    if (!manifest) {
        log_error("Failed to create manifest");
        return;
    }
    
    // Create scan context
    ScanContext ctx = {
        .manifest = manifest,
        .files_processed = 0,
        .files_with_endpoints = 0,
        .files_with_edges = 0
    };
    
    // Start timing
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Configure walker
    WalkerConfig config = walker_config_default();
    const char* python_exts[] = {"py"};
    config.extensions = python_exts;
    config.extension_count = 1;
    config.max_depth = 10;
    
    log_info("Scanning repository: %s", root_path);
    log_info("Output file: %s\n", output_file);
    
    // Walk and parse all Python files
    bool success = walker_walk(root_path, &config, parse_and_collect_callback, &ctx);
    
    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end);
    long duration_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                       (end.tv_nsec - start.tv_nsec) / 1000000;
    
    if (!success) {
        log_error("✗ Scan failed");
        manifest_free(manifest);
        return;
    }
    
    // Set scan statistics
    WalkerStats stats = walker_get_stats();
    manifest_set_stats(manifest, stats.files_matched, stats.files_ignored, duration_ms);
    
    // Display results summary
    log_info("\n========================================");
    log_info("Scan Results Summary");
    log_info("========================================");
    
    log_info("Repository: %s", manifest->repo_name);
    log_info("Scan duration: %ld ms (%.2f seconds)", duration_ms, duration_ms / 1000.0);
    log_info("");
    
    log_info("Files:");
    log_info("  Total scanned: %zu", stats.files_scanned);
    log_info("  Python files: %zu", stats.files_matched);
    log_info("  Successfully parsed: %zu", ctx.files_processed);
    log_info("  With endpoints: %zu", ctx.files_with_endpoints);
    log_info("  With dependencies: %zu", ctx.files_with_edges);
    log_info("  Ignored: %zu", stats.files_ignored);
    if (stats.errors > 0) {
        log_warn("  Errors: %zu", stats.errors);
    }
    log_info("");
    
    log_info("Architecture:");
    log_info("  Services: %zu", manifest->services->count);
    log_info("  Endpoints: %zu", manifest->endpoints->count);
    log_info("  Dependencies: %zu", manifest->edges->count);
    log_info("");
    
    // Show top services by endpoint count
    if (manifest->services->count > 0) {
        log_info("Top Services:");
        size_t top_count = manifest->services->count < 5 ? manifest->services->count : 5;
        for (size_t i = 0; i < top_count; i++) {
            Service* svc = manifest->services->items[i];
            
            // Count endpoints for this service
            size_t endpoint_count = 0;
            for (size_t j = 0; j < manifest->endpoints->count; j++) {
                if (strcmp(manifest->endpoints->items[j]->service_name, svc->name) == 0) {
                    endpoint_count++;
                }
            }
            
            log_info("  %zu. %s (%s) - %zu files, %zu endpoints", 
                     i + 1, svc->name, svc->language, svc->file_count, endpoint_count);
        }
        log_info("");
    }
    
    // Write manifest to JSON file
    log_info("Writing manifest...");
    if (manifest_write_json(manifest, output_file)) {
        log_info("✓ Manifest saved to: %s", output_file);
    } else {
        log_error("✗ Failed to write manifest");
    }
    
    log_info("\nFull scan complete!\n");
    
    manifest_free(manifest);
}

int main(int argc, char** argv) {
    logger_init(LOG_LEVEL_INFO, LOG_OUTPUT_STDOUT, NULL);
    
    log_info("========================================");
    log_info("Brightpanda v1.0.0");
    log_info("Code Architecture Analyzer");
    log_info("========================================\n");
    
    if (argc < 2) {
        log_error("Usage: %s <directory> [output_file]", argv[0]);
        log_info("Example: %s /path/to/project", argv[0]);
        log_info("Example: %s /path/to/project manifest.json", argv[0]);
        logger_shutdown();
        return 1;
    }
    
    const char* root_path = argv[1];
    const char* output_file = argc >= 3 ? argv[2] : "manifest.json";
    
    // Run all tests in sequence
    test_entity_system();
    test_walker_system(root_path);
    test_plugin_system();
    test_full_scan(root_path, output_file);
    
    // Summary
    log_info("========================================");
    log_info("All systems operational!");
    log_info("Scan complete. Check %s for results.", output_file);
    log_info("========================================");
    
    // Cleanup
    plugin_registry_shutdown();
    logger_shutdown();
    return 0;
}