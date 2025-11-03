#include "path.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

bool path_exists(const char* path) {
    if (!path) return false;
    return access(path, F_OK) == 0;
}

bool path_is_directory(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    
    return S_ISDIR(st.st_mode);
}

bool path_is_file(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    
    return S_ISREG(st.st_mode);
}

const char* path_get_extension(const char* path) {
    if (!path) return NULL;
    
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    
    // Make sure the dot is after the last slash (not in directory name)
    if (dot && (!slash || dot > slash)) {
        return dot + 1;
    }
    
    return NULL;
}

char* path_join(const char* base, const char* component) {
    if (!base || !component) return NULL;
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    
    // Check if we need a separator
    bool needs_sep = (base_len > 0 && base[base_len - 1] != '/');
    
    size_t total_len = base_len + (needs_sep ? 1 : 0) + comp_len;
    char* result = malloc(total_len + 1);
    if (!result) return NULL;
    
    strcpy(result, base);
    if (needs_sep) {
        strcat(result, "/");
    }
    strcat(result, component);
    
    return result;
}

char* path_normalize(const char* path) {
    if (!path) return NULL;
    
    // For now, just return a copy
    // TODO: Implement proper normalization with ./ and ../ handling
    size_t len = strlen(path);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strcpy(result, path);
    return result;
}

bool path_matches_pattern(const char* filename, const char* pattern) {
    if (!filename || !pattern) return false;
    
    // Simple glob matching: * matches any sequence, ? matches one char
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return true; // Trailing * matches everything
            
            // Try to match the rest of the pattern
            while (*filename) {
                if (path_matches_pattern(filename, pattern)) {
                    return true;
                }
                filename++;
            }
            return false;
        } else if (*pattern == '?') {
            if (!*filename) return false;
            pattern++;
            filename++;
        } else {
            if (*pattern != *filename) return false;
            pattern++;
            filename++;
        }
    }
    
    return *filename == '\0';
}

const char* path_basename(const char* path) {
    if (!path) return NULL;
    
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

char* path_dirname(const char* path) {
    if (!path) return NULL;
    
    // Make a copy since dirname() may modify its argument
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char* dir = dirname(path_copy);
    char* result = strdup(dir);
    
    free(path_copy);
    return result;
}