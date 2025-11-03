#ifndef BRIGHTPANDA_MANIFEST_H
#define BRIGHTPANDA_MANIFEST_H

#include "entity.h"
#include <stdbool.h>
#include <time.h>

/*
 * Manifest builder - aggregates scan results into a structured JSON output
 */

typedef struct {
    char* schema_version;
    char* repo_name;
    char* crawler_version;
    time_t timestamp;
    long scan_duration_ms;
    size_t files_analyzed;
    size_t files_skipped;
    
    ServiceList* services;
    EndpointList* endpoints;
    EdgeList* edges;
    
    char** languages;
    size_t language_count;
} Manifest;

/* Create a new manifest */
Manifest* manifest_create(const char* repo_name);

/* Load manifest from JSON file */
Manifest* manifest_load_from_json(const char* filepath);

/* Add a service to the manifest */
bool manifest_add_service(Manifest* manifest, Service* service);

/* Add an endpoint to the manifest */
bool manifest_add_endpoint(Manifest* manifest, Endpoint* endpoint);

/* Add an edge to the manifest */
bool manifest_add_edge(Manifest* manifest, Edge* edge);

/* Set scan statistics */
void manifest_set_stats(Manifest* manifest, size_t files_analyzed, 
                       size_t files_skipped, long duration_ms);

/* Write manifest to JSON file */
bool manifest_write_json(Manifest* manifest, const char* output_path);

/* Write manifest to JSON string (caller must free) */
char* manifest_to_json_string(Manifest* manifest);

/* Remove all entities associated with a specific file */
bool manifest_remove_file(Manifest* manifest, const char* filepath);

/* Free manifest */
void manifest_free(Manifest* manifest);

#endif // BRIGHTPANDA_MANIFEST_H