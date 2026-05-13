#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"
#include "parser.h"
#include "semantic.h"
#include "icg.h"

/* Main function */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source.minipy>\n", argv[0]);
        return 1;
    }

    /* Get the source file path as a command-line argument */
    const char *filepath = argv[1];

    printf("============================================================\n");
    printf("  MiniPy Compiler  —  Group 8\n");
    printf("  Source file: %s\n", filepath);
    printf("============================================================\n\n");

    /* Run Phase 1: Lexical Analysis and Phase 2: Syntax Analysis */
    ParseNode *tree = parse_file(filepath); /* The token log is printed inside parse_file */

    /* Print the parse tree */
    printf("=== PARSE TREE ===\n");
    print_tree(tree, 0);
    printf("\n");

    /* Check for parse errors */
    if (parse_error_count > 0)
    {
        printf("Parse complete: %d syntax error(s) found. "
               "Skipping semantic analysis and IC generation.\n",
               parse_error_count);
        free_tree(tree);
        return 1;
    }

    printf("Parse complete: NO syntax errors found.\n\n");

    /* Run Semantic Analysis */
    printf("=== SEMANTIC ANALYSIS ===\n");
    int sem_errors = semantic_analyse(tree);

    /* Check for semantic errors */
    if (sem_errors > 0)
    {
        printf("Semantic analysis complete: %d error(s) found. "
               "Skipping IC generation.\n",
               sem_errors);
        free_tree(tree);
        return 1;
    }

    printf("Semantic analysis complete: NO semantic errors found.\n\n");

    /* Phase 3: Intermediate Code Generation */
    icg_generate(tree); /* walks the parse tree and prints TAC */

    /* Free up allocated memory and exit */
    free_tree(tree);
    return 0;
}