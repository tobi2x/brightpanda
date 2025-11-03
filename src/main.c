#include <stdio.h>
#include <stdlib.h>
#include "core/entity.h"
#include "core/walker.h"
#include "util/logger.h"

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
    config.max_depth = 5; // Limit depth
    
    // Walk the directory
    log_info("Walking directory: %s", root_path);
    g_files_found = 0;
    
    bool success = walker_walk(root_path, &config, file_found_callback, NULL);
    
    if (success) {
        log_info("✓ Walker completed successfully");
        
        // Get statistics
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
    
    // Summary
    log_info("========================================");
    log_info("All tests complete!");
    log_info("========================================");
    
    logger_shutdown();
    return 0;
}