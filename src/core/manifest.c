#include "manifest.h"
#include "../util/logger.h"
#include "../util/path.h"
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

Manifest* manifest_load_from_json(const char* filepath) {
    if (!filepath || !path_exists(filepath)) {
        return NULL;
    }
    
    LOG_INFO("Loading previous manifest from: %s", filepath);
    
    // Read file
    FILE* file = fopen(filepath, "r");
    if (!file) {
        LOG_ERROR("Failed to open manifest file: %s", filepath);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_str = malloc(size + 1);
    if (!json_str) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(json_str, 1, size, file);
    json_str[read] = '\0';
    fclose(file);
    
    // Parse JSON
    json_object* root = json_tokener_parse(json_str);
    free(json_str);
    
    if (!root) {
        LOG_ERROR("Failed to parse manifest JSON");
        return NULL;
    }
    
    // Extract repo name
    json_object* repo_obj;
    const char* repo_name = "unknown";
    if (json_object_object_get_ex(root, "repo", &repo_obj)) {
        repo_name = json_object_get_string(repo_obj);
    }
    
    Manifest* manifest = manifest_create(repo_name);
    if (!manifest) {
        json_object_put(root);
        return NULL;
    }
    
    // Load services
    json_object* services_obj;
    if (json_object_object_get_ex(root, "services", &services_obj)) {
        size_t n_services = json_object_array_length(services_obj);
        
        for (size_t i = 0; i < n_services; i++) {
            json_object* svc_obj = json_object_array_get_idx(services_obj, i);
            
            json_object* name_obj, *lang_obj, *path_obj, *files_obj;
            if (!json_object_object_get_ex(svc_obj, "name", &name_obj) ||
                !json_object_object_get_ex(svc_obj, "language", &lang_obj) ||
                !json_object_object_get_ex(svc_obj, "path", &path_obj)) {
                continue;
            }
            
            const char* name = json_object_get_string(name_obj);
            const char* lang = json_object_get_string(lang_obj);
            const char* path = json_object_get_string(path_obj);
            
            Service* service = service_create(name, lang, path);
            if (!service) continue;
            
            // Load files
            if (json_object_object_get_ex(svc_obj, "files", &files_obj)) {
                size_t n_files = json_object_array_length(files_obj);
                for (size_t j = 0; j < n_files; j++) {
                    json_object* file_obj = json_object_array_get_idx(files_obj, j);
                    const char* file = json_object_get_string(file_obj);
                    service_add_file(service, file);
                }
            }
            
            manifest_add_service(manifest, service);
        }
    }
    
    // Load endpoints
    json_object* endpoints_obj;
    if (json_object_object_get_ex(root, "endpoints", &endpoints_obj)) {
        size_t n_endpoints = json_object_array_length(endpoints_obj);
        
        for (size_t i = 0; i < n_endpoints; i++) {
            json_object* ep_obj = json_object_array_get_idx(endpoints_obj, i);
            
            json_object* service_obj, *path_obj, *method_obj, *handler_obj;
            json_object* file_obj, *line_obj;
            
            if (!json_object_object_get_ex(ep_obj, "service", &service_obj) ||
                !json_object_object_get_ex(ep_obj, "path", &path_obj) ||
                !json_object_object_get_ex(ep_obj, "method", &method_obj)) {
                continue;
            }
            
            const char* service = json_object_get_string(service_obj);
            const char* path = json_object_get_string(path_obj);
            const char* method_str = json_object_get_string(method_obj);
            
            HttpMethod method = http_method_from_string(method_str);
            
            const char* handler = NULL;
            const char* file = NULL;
            int line = 0;
            
            if (json_object_object_get_ex(ep_obj, "handler", &handler_obj)) {
                handler = json_object_get_string(handler_obj);
            }
            
            if (json_object_object_get_ex(ep_obj, "file", &file_obj)) {
                file = json_object_get_string(file_obj);
            }
            
            if (json_object_object_get_ex(ep_obj, "line", &line_obj)) {
                line = json_object_get_int(line_obj);
            }
            
            Endpoint* endpoint = endpoint_create(service, path, method, 
                                                handler, file, line);
            if (endpoint) {
                manifest_add_endpoint(manifest, endpoint);
            }
        }
    }
    
    // Load edges
    json_object* edges_obj;
    if (json_object_object_get_ex(root, "edges", &edges_obj)) {
        size_t n_edges = json_object_array_length(edges_obj);
        
        for (size_t i = 0; i < n_edges; i++) {
            json_object* edge_obj = json_object_array_get_idx(edges_obj, i);
            
            json_object* from_obj, *to_obj, *type_obj;
            if (!json_object_object_get_ex(edge_obj, "from", &from_obj) ||
                !json_object_object_get_ex(edge_obj, "to", &to_obj) ||
                !json_object_object_get_ex(edge_obj, "type", &type_obj)) {
                continue;
            }
            
            const char* from = json_object_get_string(from_obj);
            const char* to = json_object_get_string(to_obj);
            const char* type_str = json_object_get_string(type_obj);
            
            EdgeType type = edge_type_from_string(type_str);
            
            json_object* method_obj, *endpoint_obj, *file_obj, *line_obj, *conf_obj;
            
            const char* method = NULL;
            const char* endpoint = NULL;
            const char* file = NULL;
            int line = 0;
            float confidence = 1.0f;
            
            if (json_object_object_get_ex(edge_obj, "method", &method_obj)) {
                method = json_object_get_string(method_obj);
            }
            
            if (json_object_object_get_ex(edge_obj, "endpoint", &endpoint_obj)) {
                endpoint = json_object_get_string(endpoint_obj);
            }
            
            if (json_object_object_get_ex(edge_obj, "file", &file_obj)) {
                file = json_object_get_string(file_obj);
            }
            
            if (json_object_object_get_ex(edge_obj, "line", &line_obj)) {
                line = json_object_get_int(line_obj);
            }
            
            if (json_object_object_get_ex(edge_obj, "confidence", &conf_obj)) {
                confidence = json_object_get_double(conf_obj);
            }
            
            Edge* edge = edge_create(from, to, type, method, endpoint, file, line);
            if (edge) {
                edge_set_confidence(edge, confidence);
                manifest_add_edge(manifest, edge);
            }
        }
    }
    
    json_object_put(root);
    
    LOG_INFO("Loaded manifest: %zu services, %zu endpoints, %zu edges",
             manifest->services->count,
             manifest->endpoints->count,
             manifest->edges->count);
    
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

bool manifest_remove_file(Manifest* manifest, const char* filepath) {
    if (!manifest || !filepath) return false;
    
    const char* basename = path_basename(filepath);
    if (!basename) return false;
    
    LOG_DEBUG("Removing entities for deleted file: %s", filepath);
    
    // Remove endpoints from this file
    for (size_t i = 0; i < manifest->endpoints->count; ) {
        Endpoint* ep = manifest->endpoints->items[i];
        if (ep->file && strcmp(ep->file, basename) == 0) {
            // Remove this endpoint
            endpoint_free(ep);
            // Shift remaining items
            for (size_t j = i; j < manifest->endpoints->count - 1; j++) {
                manifest->endpoints->items[j] = manifest->endpoints->items[j + 1];
            }
            manifest->endpoints->count--;
        } else {
            i++;
        }
    }
    
    // Remove edges from this file
    for (size_t i = 0; i < manifest->edges->count; ) {
        Edge* edge = manifest->edges->items[i];
        if (edge->file && strcmp(edge->file, basename) == 0) {
            // Remove this edge
            edge_free(edge);
            // Shift remaining items
            for (size_t j = i; j < manifest->edges->count - 1; j++) {
                manifest->edges->items[j] = manifest->edges->items[j + 1];
            }
            manifest->edges->count--;
        } else {
            i++;
        }
    }
    
    // Remove file from services
    for (size_t i = 0; i < manifest->services->count; i++) {
        Service* svc = manifest->services->items[i];
        service_remove_file(svc, filepath);
    }
    
    return true;
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