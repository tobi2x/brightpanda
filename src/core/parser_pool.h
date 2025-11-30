#ifndef BRIGHTPANDA_PARSER_POOL_H
#define BRIGHTPANDA_PARSER_POOL_H

#include <tree_sitter/api.h>
#include <stdbool.h>

/*
 * Parser pool manages reusable Tree-sitter parser instances.
 * Reduces overhead of creating/destroying parsers for each file.
 */

/* Initialize the parser pool */
bool parser_pool_init(void);

/* Get a parser for a specific language */
TSParser* parser_pool_acquire(const char* language);

/* Return a parser to the pool */
void parser_pool_release(TSParser* parser);

/* External Tree-sitter language declarations */
extern const TSLanguage *tree_sitter_python(void);

/* Pool state */
#define MAX_PARSERS 8

static struct {
    TSParser* parsers[MAX_PARSERS];
    bool in_use[MAX_PARSERS];
    const TSLanguage* language;
    size_t count;
    bool initialized;
} pool_state = {0};


/* Cleanup the parser pool */
void parser_pool_shutdown(void);

#endif // BRIGHTPANDA_PARSER_POOL_H