#ifndef BRIGHTPANDA_WALKER_H
#define BRIGHTPANDA_WALKER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Directory walker for traversing repositories.
 * Respects .gitignore patterns and finds relevant source files.
 */

/* Callback function for each file found */
typedef void (*walker_file_callback)(const char* filepath, void* userdata);

/* Walker configuration */
typedef struct {
    bool follow_symlinks;        // Follow symbolic links
    bool respect_gitignore;      // Honor .gitignore files
    size_t max_depth;            // Maximum directory depth (0 = unlimited)
    const char** extensions;     // File extensions to include (NULL-terminated)
    size_t extension_count;      // Number of extensions
} WalkerConfig;

/* Initialize walker with default configuration */
WalkerConfig walker_config_default(void);

/* Walk a directory tree and call callback for each matching file */
bool walker_walk(
    const char* root_path,
    const WalkerConfig* config,
    walker_file_callback callback,
    void* userdata
);

/* Check if a file should be ignored based on .gitignore patterns */
bool walker_should_ignore(const char* path, const char* filename);

/* Statistics collected during walk */
typedef struct {
    size_t files_scanned;
    size_t files_matched;
    size_t files_ignored;
    size_t directories_visited;
    size_t errors;
} WalkerStats;

/* Get statistics from last walk (must be called immediately after walker_walk) */
WalkerStats walker_get_stats(void);

#endif // BRIGHTPANDA_WALKER_H