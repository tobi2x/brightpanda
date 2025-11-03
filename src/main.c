#include <stdio.h>
#include <stdlib.h>
#include "core/entity.h"
#include "util/logger.h"

int main(int argc, char** argv) {
    // Initialize logger
    logger_init(LOG_LEVEL_DEBUG, LOG_OUTPUT_STDOUT, NULL);
    
    log_info("Brightpanda v1.0.0");
    
    if (argc < 2) {
        log_error("Usage: %s <directory>", argv[0]);
        return 1;
    }
    
    log_info("Scanning directory: %s", argv[1]);
    
    // Test entity system
    LOG_DEBUG("Testing entity system...");
    
    // Create a service
    Service* service = service_create("auth-service", "python", "./services/auth");
    if (service) {
        log_info("Created service: %s (%s)", service->name, service->language);
        service_add_file(service, "services/auth/main.py");
        service_add_file(service, "services/auth/routes.py");
        LOG_DEBUG("Added %zu files to service", service->file_count);
        service_free(service);
    }
    
    // Create an endpoint
    Endpoint* endpoint = endpoint_create("auth-service", "/api/login", HTTP_POST, "login_handler", "routes.py", 42);
    if (endpoint) {
        log_info("Created endpoint: %s %s", http_method_to_string(endpoint->method), endpoint->path);
        endpoint_free(endpoint);
    }
    
    // Create an edge
    Edge* edge = edge_create("auth-service", "user-db", EDGE_DATABASE, NULL, NULL, "main.py", 15);
    if (edge) {
        log_info("Created edge: %s -> %s (%s)", edge->from_service, edge->to_service, edge_type_to_string(edge->type));
        edge_free(edge);
    }
    
    log_warn("Full implementation not yet complete");
    
    logger_shutdown();
    return 0;
}