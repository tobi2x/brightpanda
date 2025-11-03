#include "entity.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===== HELPER FUNCTIONS ===== */

static char* strdup_safe(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, str, len + 1);
    return copy;
}

/* ===== SERVICE IMPLEMENTATION ===== */

Service* service_create(const char* name, const char* language, const char* path) {
    if (!name || !language) return NULL;
    
    Service* service = calloc(1, sizeof(Service));
    if (!service) return NULL;
    
    service->name = strdup_safe(name);
    service->language = strdup_safe(language);
    service->path = strdup_safe(path);
    
    if (!service->name || !service->language) {
        service_free(service);
        return NULL;
    }
    
    service->file_capacity = 16;
    service->files = calloc(service->file_capacity, sizeof(char*));
    if (!service->files) {
        service_free(service);
        return NULL;
    }
    
    return service;
}

bool service_add_file(Service* service, const char* filepath) {
    if (!service || !filepath) return false;
    
    // Resize if needed
    if (service->file_count >= service->file_capacity) {
        size_t new_capacity = service->file_capacity * 2;
        char** new_files = realloc(service->files, new_capacity * sizeof(char*));
        if (!new_files) return false;
        service->files = new_files;
        service->file_capacity = new_capacity;
    }
    
    service->files[service->file_count] = strdup_safe(filepath);
    if (!service->files[service->file_count]) return false;
    
    service->file_count++;
    return true;
}

void service_free(Service* service) {
    if (!service) return;
    
    free(service->name);
    free(service->language);
    free(service->path);
    
    if (service->files) {
        for (size_t i = 0; i < service->file_count; i++) {
            free(service->files[i]);
        }
        free(service->files);
    }
    
    free(service);
}

Service* service_clone(const Service* service) {
    if (!service) return NULL;
    
    Service* clone = service_create(service->name, service->language, service->path);
    if (!clone) return NULL;
    
    for (size_t i = 0; i < service->file_count; i++) {
        if (!service_add_file(clone, service->files[i])) {
            service_free(clone);
            return NULL;
        }
    }
    
    return clone;
}

int service_compare(const Service* a, const Service* b) {
    if (!a || !b) return 0;
    return strcmp(a->name, b->name);
}

/* ===== ENDPOINT IMPLEMENTATION ===== */

Endpoint* endpoint_create(
    const char* service_name,
    const char* path,
    HttpMethod method,
    const char* handler,
    const char* file,
    int line
) {
    if (!service_name || !path) return NULL;
    
    Endpoint* endpoint = calloc(1, sizeof(Endpoint));
    if (!endpoint) return NULL;
    
    endpoint->service_name = strdup_safe(service_name);
    endpoint->path = strdup_safe(path);
    endpoint->method = method;
    endpoint->handler = strdup_safe(handler);
    endpoint->file = strdup_safe(file);
    endpoint->line = line;
    
    if (!endpoint->service_name || !endpoint->path) {
        endpoint_free(endpoint);
        return NULL;
    }
    
    return endpoint;
}

void endpoint_free(Endpoint* endpoint) {
    if (!endpoint) return;
    
    free(endpoint->service_name);
    free(endpoint->path);
    free(endpoint->handler);
    free(endpoint->file);
    free(endpoint);
}

Endpoint* endpoint_clone(const Endpoint* endpoint) {
    if (!endpoint) return NULL;
    
    return endpoint_create(
        endpoint->service_name,
        endpoint->path,
        endpoint->method,
        endpoint->handler,
        endpoint->file,
        endpoint->line
    );
}

int endpoint_compare(const Endpoint* a, const Endpoint* b) {
    if (!a || !b) return 0;
    
    int cmp = strcmp(a->service_name, b->service_name);
    if (cmp != 0) return cmp;
    
    cmp = strcmp(a->path, b->path);
    if (cmp != 0) return cmp;
    
    return (int)a->method - (int)b->method;
}

HttpMethod http_method_from_string(const char* str) {
    if (!str) return HTTP_UNKNOWN;
    
    if (strcasecmp(str, "GET") == 0) return HTTP_GET;
    if (strcasecmp(str, "POST") == 0) return HTTP_POST;
    if (strcasecmp(str, "PUT") == 0) return HTTP_PUT;
    if (strcasecmp(str, "DELETE") == 0) return HTTP_DELETE;
    if (strcasecmp(str, "PATCH") == 0) return HTTP_PATCH;
    if (strcasecmp(str, "HEAD") == 0) return HTTP_HEAD;
    if (strcasecmp(str, "OPTIONS") == 0) return HTTP_OPTIONS;
    
    return HTTP_UNKNOWN;
}

const char* http_method_to_string(HttpMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_HEAD: return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

/* ===== EDGE IMPLEMENTATION ===== */

Edge* edge_create(
    const char* from_service,
    const char* to_service,
    EdgeType type,
    const char* method,
    const char* endpoint,
    const char* file,
    int line
) {
    if (!from_service || !to_service) return NULL;
    
    Edge* edge = calloc(1, sizeof(Edge));
    if (!edge) return NULL;
    
    edge->from_service = strdup_safe(from_service);
    edge->to_service = strdup_safe(to_service);
    edge->type = type;
    edge->method = strdup_safe(method);
    edge->endpoint = strdup_safe(endpoint);
    edge->file = strdup_safe(file);
    edge->line = line;
    edge->confidence = 1.0f; // Default to high confidence
    
    if (!edge->from_service || !edge->to_service) {
        edge_free(edge);
        return NULL;
    }
    
    return edge;
}

bool service_remove_file(Service* service, const char* filepath) {
    if (!service || !filepath) return false;
    
    for (size_t i = 0; i < service->file_count; i++) {
        if (strcmp(service->files[i], filepath) == 0) {
            // Found the file, remove it
            free(service->files[i]);
            
            // Shift remaining files
            for (size_t j = i; j < service->file_count - 1; j++) {
                service->files[j] = service->files[j + 1];
            }
            
            service->file_count--;
            service->files[service->file_count] = NULL;
            
            return true;
        }
    }
    
    return false;
}

void edge_set_confidence(Edge* edge, float confidence) {
    if (!edge) return;
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;
    edge->confidence = confidence;
}

void edge_free(Edge* edge) {
    if (!edge) return;
    
    free(edge->from_service);
    free(edge->to_service);
    free(edge->method);
    free(edge->endpoint);
    free(edge->file);
    free(edge);
}

Edge* edge_clone(const Edge* edge) {
    if (!edge) return NULL;
    
    Edge* clone = edge_create(
        edge->from_service,
        edge->to_service,
        edge->type,
        edge->method,
        edge->endpoint,
        edge->file,
        edge->line
    );
    
    if (clone) {
        clone->confidence = edge->confidence;
    }
    
    return clone;
}

int edge_compare(const Edge* a, const Edge* b) {
    if (!a || !b) return 0;
    
    int cmp = strcmp(a->from_service, b->from_service);
    if (cmp != 0) return cmp;
    
    cmp = strcmp(a->to_service, b->to_service);
    if (cmp != 0) return cmp;
    
    return (int)a->type - (int)b->type;
}

EdgeType edge_type_from_string(const char* str) {
    if (!str) return EDGE_UNKNOWN;
    
    if (strcasecmp(str, "HTTP") == 0 || strcasecmp(str, "HTTP_CALL") == 0) {
        return EDGE_HTTP_CALL;
    }
    if (strcasecmp(str, "IMPORT") == 0) return EDGE_IMPORT;
    if (strcasecmp(str, "RPC") == 0) return EDGE_RPC;
    if (strcasecmp(str, "DATABASE") == 0 || strcasecmp(str, "DB") == 0) {
        return EDGE_DATABASE;
    }
    if (strcasecmp(str, "MESSAGE_QUEUE") == 0 || strcasecmp(str, "MQ") == 0) {
        return EDGE_MESSAGE_QUEUE;
    }
    
    return EDGE_UNKNOWN;
}

const char* edge_type_to_string(EdgeType type) {
    switch (type) {
        case EDGE_HTTP_CALL: return "HTTP_CALL";
        case EDGE_IMPORT: return "IMPORT";
        case EDGE_RPC: return "RPC";
        case EDGE_DATABASE: return "DATABASE";
        case EDGE_MESSAGE_QUEUE: return "MESSAGE_QUEUE";
        default: return "UNKNOWN";
    }
}

/* ===== COLLECTION IMPLEMENTATIONS ===== */

#define INITIAL_CAPACITY 32

ServiceList* service_list_create(void) {
    ServiceList* list = calloc(1, sizeof(ServiceList));
    if (!list) return NULL;
    
    list->capacity = INITIAL_CAPACITY;
    list->items = calloc(list->capacity, sizeof(Service*));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    return list;
}

bool service_list_add(ServiceList* list, Service* service) {
    if (!list || !service) return false;
    
    // Check for duplicates
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i]->name, service->name) == 0) {
            return false; // Duplicate
        }
    }
    
    // Resize if needed
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        Service** new_items = realloc(list->items, new_capacity * sizeof(Service*));
        if (!new_items) return false;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = service;
    return true;
}

Service* service_list_find(ServiceList* list, const char* name) {
    if (!list || !name) return NULL;
    
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i]->name, name) == 0) {
            return list->items[i];
        }
    }
    
    return NULL;
}

void service_list_free(ServiceList* list) {
    if (!list) return;
    
    if (list->items) {
        for (size_t i = 0; i < list->count; i++) {
            service_free(list->items[i]);
        }
        free(list->items);
    }
    
    free(list);
}

EndpointList* endpoint_list_create(void) {
    EndpointList* list = calloc(1, sizeof(EndpointList));
    if (!list) return NULL;
    
    list->capacity = INITIAL_CAPACITY;
    list->items = calloc(list->capacity, sizeof(Endpoint*));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    return list;
}

bool endpoint_list_add(EndpointList* list, Endpoint* endpoint) {
    if (!list || !endpoint) return false;
    
    // Resize if needed
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        Endpoint** new_items = realloc(list->items, new_capacity * sizeof(Endpoint*));
        if (!new_items) return false;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = endpoint;
    return true;
}

void endpoint_list_free(EndpointList* list) {
    if (!list) return;
    
    if (list->items) {
        for (size_t i = 0; i < list->count; i++) {
            endpoint_free(list->items[i]);
        }
        free(list->items);
    }
    
    free(list);
}

EdgeList* edge_list_create(void) {
    EdgeList* list = calloc(1, sizeof(EdgeList));
    if (!list) return NULL;
    
    list->capacity = INITIAL_CAPACITY;
    list->items = calloc(list->capacity, sizeof(Edge*));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    return list;
}

bool edge_list_add(EdgeList* list, Edge* edge) {
    if (!list || !edge) return false;
    
    // Resize if needed
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        Edge** new_items = realloc(list->items, new_capacity * sizeof(Edge*));
        if (!new_items) return false;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = edge;
    return true;
}

void edge_list_free(EdgeList* list) {
    if (!list) return;
    
    if (list->items) {
        for (size_t i = 0; i < list->count; i++) {
            edge_free(list->items[i]);
        }
        free(list->items);
    }
    
    free(list);
}