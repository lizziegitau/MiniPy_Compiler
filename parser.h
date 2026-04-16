#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"

// Parse Tree node structure.  Each node has a label (non-terminal name or token lexeme) and a flag indicating if it's a terminal (leaf) or non-terminal (inner node).  Each node can have up to MAX_CHILDREN (8) children.
#define MAX_CHILDREN 8
#define MAX_LABEL 128

typedef struct ParseNode
{
   char label[MAX_LABEL]; // Non-terminal name or token lexeme
   int is_terminal;       // 1 = leaf (token), 0 = inner node
   struct ParseNode *children[MAX_CHILDREN];
   int child_count;
} ParseNode;

// Public API for the parser module.
// Initialise the parser with a source file path. Loads and scans the file, logs the token stream, then parses. Returns the root of the parse tree, or NULL on fatal error
ParseNode *parse_file(const char *path);

// Uses depth to control indentation when printing the tree.  Call with depth=0 for the root.  Prints the tree structure to stdout
void print_tree(ParseNode *node, int depth);

// Free all memory used by the parse tree
void free_tree(ParseNode *node);

// Total syntax errors encountered (0 = clean parse).
extern int parse_error_count;

#endif