#include "manifest.h"
#include "../util/logger.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCHEMA_VERSION "1.0"
#define CRAWLER_VERSION "1.0.0"

Manifest* manifest_create(const char* repo_name) {
    Manifest* manifest = calloc(1, sizeof(Manifest));
    if (!manifest) return NULL;
    
    manifest->schema_version = strdup(SCHEMA_VERSION);
    manifest->crawler_version = strdup(CRAWLER_VERSION);
    manifest->repo_name = repo_name ? strdup(repo_name) : strdup("unknown");
    manifest->timestamp = time(NULL);
    
    manifest->services = service_list_create();
    manifest->endpoints = endpoint_list_create();
    manifest->edges = edge_list_create();
    
    if (!manifest->services || !manifest->endpoints || !manifest->edges) {
        manifest_free(manifest);
        return NULL;
    }
    
    return manifest;
}

bool manifest_add_service(Manifest* manifest, Service* service) {
    if (!manifest || !service) return false;
    return service_list_add(manifest->services, service);
}

bool manifest_add_endpoint(Manifest* manifest, Endpoint* endpoint) {
    if (!manifest || !endpoint) return false;
    return endpoint_list_add(manifest->endpoints, endpoint);
}

bool manifest_add_edge(Manifest* manifest, Edge* edge) {
    if (!manifest || !edge) return false;
    return edge_list_add(manifest->edges, edge);
}

void manifest_set_stats(Manifest* manifest, size_t files_analyzed,
                       size_t files_skipped, long duration_ms) {
    if (!manifest) return;
    manifest->files_analyzed = files_analyzed;
    manifest->files_skipped = files_skipped;
    manifest->scan_duration_ms = duration_ms;
}

static json_object* service_to_json(const Service* service) {
    json_object* obj = json_object_new_object();
    
    json_object_object_add(obj, "name", json_object_new_string(service->name));
    json_object_object_add(obj, "language", json_object_new_string(service->language));
    json_object_object_add(obj, "path", json_object_new_string(service->path));
    json_object_object_add(obj, "file_count", json_object_new_int64(service->file_count));
    
    // Add files array
    json_object* files_array = json_object_new_array();
    for (size_t i = 0; i < service->file_count; i++) {
        json_object_array_add(files_array, json_object_new_string(service->files[i]));
    }
    json_object_object_add(obj, "files", files_array);
    
    return obj;
}

static json_object* endpoint_to_json(const Endpoint* endpoint) {
    json_object* obj = json_object_new_object();
    
    json_object_object_add(obj, "service", json_object_new_string(endpoint->service_name));
    json_object_object_add(obj, "path", json_object_new_string(endpoint->path));
    json_object_object_add(obj, "method", json_object_new_string(http_method_to_string(endpoint->method)));
    
    if (endpoint->handler) {
        json_object_object_add(obj, "handler", json_object_new_string(endpoint->handler));
    }
    
    if (endpoint->file) {
        json_object_object_add(obj, "file", json_object_new_string(endpoint->file));
        json_object_object_add(obj, "line", json_object_new_int(endpoint->line));
    }
    
    return obj;
}

static json_object* edge_to_json(const Edge* edge) {
    json_object* obj = json_object_new_object();
    
    json_object_object_add(obj, "from", json_object_new_string(edge->from_service));
    json_object_object_add(obj, "to", json_object_new_string(edge->to_service));
    json_object_object_add(obj, "type", json_object_new_string(edge_type_to_string(edge->type)));
    
    if (edge->method) {
        json_object_object_add(obj, "method", json_object_new_string(edge->method));
    }
    
    if (edge->endpoint) {
        json_object_object_add(obj, "endpoint", json_object_new_string(edge->endpoint));
    }
    
    if (edge->file) {
        json_object_object_add(obj, "file", json_object_new_string(edge->file));
        json_object_object_add(obj, "line", json_object_new_int(edge->line));
    }
    
    json_object_object_add(obj, "confidence", json_object_new_double(edge->confidence));
    
    return obj;
}

char* manifest_to_json_string(Manifest* manifest) {
    if (!manifest) return NULL;
    
    json_object* root = json_object_new_object();
    
    // Schema version
    json_object_object_add(root, "schema_version", 
                          json_object_new_string(manifest->schema_version));
    
    // Metadata
    json_object* metadata = json_object_new_object();
    
    char timestamp_str[64];
    struct tm* tm_info = localtime(&manifest->timestamp);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    json_object_object_add(metadata, "timestamp", json_object_new_string(timestamp_str));
    
    json_object_object_add(metadata, "crawler_version", 
                          json_object_new_string(manifest->crawler_version));
    json_object_object_add(metadata, "scan_duration_ms", 
                          json_object_new_int64(manifest->scan_duration_ms));
    json_object_object_add(metadata, "files_analyzed", 
                          json_object_new_int64(manifest->files_analyzed));
    json_object_object_add(metadata, "files_skipped", 
                          json_object_new_int64(manifest->files_skipped));
    
    json_object_object_add(root, "scan_metadata", metadata);
    
    // Repository name
    json_object_object_add(root, "repo", json_object_new_string(manifest->repo_name));
    
    // Collect unique languages
    json_object* languages = json_object_new_array();
    // Simple approach: just add "python" for now since that's all we support
    json_object_array_add(languages, json_object_new_string("python"));
    json_object_object_add(root, "languages", languages);
    
    // Services
    json_object* services = json_object_new_array();
    for (size_t i = 0; i < manifest->services->count; i++) {
        json_object_array_add(services, service_to_json(manifest->services->items[i]));
    }
    json_object_object_add(root, "services", services);
    
    // Endpoints
    json_object* endpoints = json_object_new_array();
    for (size_t i = 0; i < manifest->endpoints->count; i++) {
        json_object_array_add(endpoints, endpoint_to_json(manifest->endpoints->items[i]));
    }
    json_object_object_add(root, "endpoints", endpoints);
    
    // Edges
    json_object* edges = json_object_new_array();
    for (size_t i = 0; i < manifest->edges->count; i++) {
        json_object_array_add(edges, edge_to_json(manifest->edges->items[i]));
    }
    json_object_object_add(root, "edges", edges);
    
    // Convert to string (pretty printed)
    const char* json_str = json_object_to_json_string_ext(root, 
                                                           JSON_C_TO_STRING_PRETTY | 
                                                           JSON_C_TO_STRING_SPACED);
    char* result = strdup(json_str);
    
    json_object_put(root);
    
    return result;
}

bool manifest_write_json(Manifest* manifest, const char* output_path) {
    if (!manifest || !output_path) return false;
    
    LOG_INFO("Writing manifest to: %s", output_path);
    
    char* json_str = manifest_to_json_string(manifest);
    if (!json_str) {
        LOG_ERROR("Failed to generate JSON string");
        return false;
    }
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to open file for writing: %s", output_path);
        free(json_str);
        return false;
    }
    
    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, file);
    fclose(file);
    free(json_str);
    
    if (written != len) {
        LOG_ERROR("Failed to write complete manifest");
        return false;
    }
    
    LOG_INFO("Manifest written successfully (%zu bytes)", written);
    return true;
}

void manifest_free(Manifest* manifest) {
    if (!manifest) return;
    
    free(manifest->schema_version);
    free(manifest->crawler_version);
    free(manifest->repo_name);
    
    if (manifest->languages) {
        for (size_t i = 0; i < manifest->language_count; i++) {
            free(manifest->languages[i]);
        }
        free(manifest->languages);
    }
    
    service_list_free(manifest->services);
    endpoint_list_free(manifest->endpoints);
    edge_list_free(manifest->edges);
    
    free(manifest);
}