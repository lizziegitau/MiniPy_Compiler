#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"

/* Enumerate the possible MiniPy types */
typedef enum
{
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL
} MiniType;

/* Return a human-readable string for a MiniType value. */
const char *type_name(MiniType t);

/* Define symbol table size and maximum name length */
#define SYM_TABLE_SIZE 128
#define SYM_NAME_MAX 64

/* Structure for symbol table entries */
typedef struct
{
    char name[SYM_NAME_MAX]; /* variable name */
    MiniType type;           /* inferred type */
    int defined;             /* Flag indicating if the symbol is defined */
} Symbol;

/* Run semantic analysis on the parse tree from the root node */
int semantic_analyse(ParseNode *root);

/* Look up a variable name in the symbol table */
const Symbol *sym_lookup(const char *name);

/*  Print the symbol table to stdout (called by semantic_analyse) */
void sym_table_print(void);

/* Total semantic errors found during the last call to semantic_analyse() */
extern int semantic_error_count;

#endif