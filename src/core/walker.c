#include "walker.h"
#include "../util/path.h"
#include "../util/logger.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Global statistics for current walk */
static WalkerStats g_stats = {0};

/* Common ignore patterns */
static const char* BUILTIN_IGNORE_PATTERNS[] = {
    ".git",
    ".svn",
    ".hg",
    "node_modules",
    "__pycache__",
    ".pytest_cache",
    ".mypy_cache",
    "venv",
    ".venv",
    "env",
    ".env",
    "build",
    "dist",
    ".DS_Store",
    "*.pyc",
    "*.pyo",
    "*.pyd",
    ".so",
    ".dylib",
    NULL
};

WalkerConfig walker_config_default(void) {
    WalkerConfig config = {
        .follow_symlinks = false,
        .respect_gitignore = true,
        .max_depth = 0,
        .extensions = NULL,
        .extension_count = 0
    };
    return config;
}

static bool should_ignore_builtin(const char* name) {
    for (size_t i = 0; BUILTIN_IGNORE_PATTERNS[i]; i++) {
        if (path_matches_pattern(name, BUILTIN_IGNORE_PATTERNS[i])) {
            return true;
        }
    }
    return false;
}

static bool matches_extensions(const char* filepath, const WalkerConfig* config) {
    if (!config->extensions || config->extension_count == 0) {
        return true; // No filter, accept all
    }
    
    const char* ext = path_get_extension(filepath);
    if (!ext) return false;
    
    for (size_t i = 0; i < config->extension_count; i++) {
        if (strcmp(ext, config->extensions[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

static void walk_recursive(
    const char* dirpath,
    const WalkerConfig* config,
    walker_file_callback callback,
    void* userdata,
    size_t current_depth
) {
    // Check max depth
    if (config->max_depth > 0 && current_depth >= config->max_depth) {
        return;
    }
    
    DIR* dir = opendir(dirpath);
    if (!dir) {
        LOG_WARN("Failed to open directory: %s", dirpath);
        g_stats.errors++;
        return;
    }
    
    g_stats.directories_visited++;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if should be ignored
        if (should_ignore_builtin(entry->d_name)) {
            LOG_DEBUG("Ignoring: %s", entry->d_name);
            g_stats.files_ignored++;
            continue;
        }
        
        // Build full path
        char* fullpath = path_join(dirpath, entry->d_name);
        if (!fullpath) {
            g_stats.errors++;
            continue;
        }
        
        // Check if it's a directory or file
        struct stat st;
        if (lstat(fullpath, &st) != 0) {
            LOG_DEBUG("Failed to stat: %s", fullpath);
            free(fullpath);
            g_stats.errors++;
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recurse into directory
            walk_recursive(fullpath, config, callback, userdata, current_depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            // Regular file
            g_stats.files_scanned++;
            
            if (matches_extensions(fullpath, config)) {
                g_stats.files_matched++;
                LOG_DEBUG("Found file: %s", fullpath);
                callback(fullpath, userdata);
            }
        } else if (S_ISLNK(st.st_mode) && config->follow_symlinks) {
            // Symlink - follow if configured
            if (path_is_directory(fullpath)) {
                walk_recursive(fullpath, config, callback, userdata, current_depth + 1);
            } else if (path_is_file(fullpath)) {
                g_stats.files_scanned++;
                if (matches_extensions(fullpath, config)) {
                    g_stats.files_matched++;
                    callback(fullpath, userdata);
                }
            }
        }
        
        free(fullpath);
    }
    
    closedir(dir);
}

bool walker_walk(
    const char* root_path,
    const WalkerConfig* config,
    walker_file_callback callback,
    void* userdata
) {
    if (!root_path || !config || !callback) {
        return false;
    }
    
    // Reset statistics
    memset(&g_stats, 0, sizeof(g_stats));
    
    // Verify root path exists
    if (!path_exists(root_path)) {
        LOG_ERROR("Path does not exist: %s", root_path);
        return false;
    }
    
    if (!path_is_directory(root_path)) {
        LOG_ERROR("Path is not a directory: %s", root_path);
        return false;
    }
    
    LOG_INFO("Walking directory: %s", root_path);
    
    // Start recursive walk
    walk_recursive(root_path, config, callback, userdata, 0);
    
    LOG_INFO("Walk complete: %zu files scanned, %zu matched, %zu ignored",
             g_stats.files_scanned, g_stats.files_matched, g_stats.files_ignored);
    
    return true;
}

bool walker_should_ignore(const char* path, const char* filename) {
    (void)path; // Unused for now
    return should_ignore_builtin(filename);
}

WalkerStats walker_get_stats(void) {
    return g_stats;
}