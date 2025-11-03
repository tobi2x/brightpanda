#ifndef BRIGHTPANDA_PATH_H
#define BRIGHTPANDA_PATH_H

#include <stdbool.h>

/* Check if a path exists */
bool path_exists(const char* path);

/* Check if a path is a directory */
bool path_is_directory(const char* path);

/* Check if a path is a regular file */
bool path_is_file(const char* path);

/* Get the file extension (returns pointer to extension or NULL) */
const char* path_get_extension(const char* path);

/* Join two path components with proper separator */
char* path_join(const char* base, const char* component);

/* Normalize a path (remove ./ and ../) */
char* path_normalize(const char* path);

/* Check if a filename matches a pattern (simple glob with * and ?) */
bool path_matches_pattern(const char* filename, const char* pattern);

/* Get basename (filename without directory) */
const char* path_basename(const char* path);

/* Get directory name (path without filename) */
char* path_dirname(const char* path);

#endif // BRIGHTPANDA_PATH_H