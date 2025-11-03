#ifndef BRIGHTPANDA_ENTITY_H
#define BRIGHTPANDA_ENTITY_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Core entity types for Brightpanda's manifest.
 * These represent the fundamental building blocks of a repository's architecture.
 */

/* ===== SERVICE ===== */

typedef struct {
    char* name;           // Service identifier (e.g., "auth-service")
    char* language;       // Primary language (e.g., "python", "go")
    char* path;           // Relative path from repo root
    char** files;         // Array of file paths belonging to this service
    size_t file_count;    // Number of files
    size_t file_capacity; // Allocated capacity for files array
} Service;

/* Create a new service */
Service* service_create(const char* name, const char* language, const char* path);

/* Add a file to a service */
bool service_add_file(Service* service, const char* filepath);

/* Free service memory */
void service_free(Service* service);

/* Clone a service (deep copy) */
Service* service_clone(const Service* service);

/* Compare two services (by name) */
int service_compare(const Service* a, const Service* b);

/* ===== ENDPOINT ===== */

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_UNKNOWN
} HttpMethod;

typedef struct {
    char* service_name;   // Which service owns this endpoint
    char* path;           // Route path (e.g., "/api/login")
    HttpMethod method;    // HTTP method
    char* handler;        // Function/handler name (optional)
    char* file;           // Source file where defined
    int line;             // Line number in source file
} Endpoint;

/* Create a new endpoint */
Endpoint* endpoint_create(
    const char* service_name,
    const char* path,
    HttpMethod method,
    const char* handler,
    const char* file,
    int line
);

/* Free endpoint memory */
void endpoint_free(Endpoint* endpoint);

/* Clone an endpoint (deep copy) */
Endpoint* endpoint_clone(const Endpoint* endpoint);

/* Compare two endpoints (by service + path + method) */
int endpoint_compare(const Endpoint* a, const Endpoint* b);

/* Convert HTTP method string to enum */
HttpMethod http_method_from_string(const char* str);

/* Convert HTTP method enum to string */
const char* http_method_to_string(HttpMethod method);

/* ===== EDGE (Dependency) ===== */

typedef enum {
    EDGE_HTTP_CALL,      // HTTP client request
    EDGE_IMPORT,         // Module import
    EDGE_RPC,            // RPC call
    EDGE_DATABASE,       // Database connection
    EDGE_MESSAGE_QUEUE,  // Message queue publish/subscribe
    EDGE_UNKNOWN
} EdgeType;

typedef struct {
    char* from_service;   // Source service making the call
    char* to_service;     // Target service or external dependency
    EdgeType type;        // Type of dependency
    char* method;         // HTTP method or call type (optional)
    char* endpoint;       // Target endpoint path (optional)
    char* file;           // Source file where call originates
    int line;             // Line number in source file
    float confidence;     // Confidence score (0.0-1.0) for inferred edges
} Edge;

/* Create a new edge */
Edge* edge_create(
    const char* from_service,
    const char* to_service,
    EdgeType type,
    const char* method,
    const char* endpoint,
    const char* file,
    int line
);

/* Set confidence score for an edge */
void edge_set_confidence(Edge* edge, float confidence);

/* Free edge memory */
void edge_free(Edge* edge);

/* Clone an edge (deep copy) */
Edge* edge_clone(const Edge* edge);

/* Compare two edges (by from + to + type) */
int edge_compare(const Edge* a, const Edge* b);

/* Convert edge type string to enum */
EdgeType edge_type_from_string(const char* str);

/* Convert edge type enum to string */
const char* edge_type_to_string(EdgeType type);

/* ===== COLLECTION UTILITIES ===== */

/* Dynamic array for services */
typedef struct {
    Service** items;
    size_t count;
    size_t capacity;
} ServiceList;

ServiceList* service_list_create(void);
bool service_list_add(ServiceList* list, Service* service);
Service* service_list_find(ServiceList* list, const char* name);
void service_list_free(ServiceList* list);

/* Dynamic array for endpoints */
typedef struct {
    Endpoint** items;
    size_t count;
    size_t capacity;
} EndpointList;

EndpointList* endpoint_list_create(void);
bool endpoint_list_add(EndpointList* list, Endpoint* endpoint);
void endpoint_list_free(EndpointList* list);

/* Dynamic array for edges */
typedef struct {
    Edge** items;
    size_t count;
    size_t capacity;
} EdgeList;

EdgeList* edge_list_create(void);
bool edge_list_add(EdgeList* list, Edge* edge);
void edge_list_free(EdgeList* list);

#endif // BRIGHTPANDA_ENTITY_H