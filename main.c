#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"
#include "parser.h"
#include "icg.h"

/* Main function */
int main(int argc, char *argv[])
{
    /* Check if the correct number of arguments is provided */
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source.minipy>\n", argv[0]);
        return 1;
    }

    /* Get the source file path which is the .minipy file passed by the user */
    const char *filepath = argv[1];

    /* Print compiler information */
    printf("============================================================\n");
    printf("  MiniPy Compiler  —  Group 8\n");
    printf("  Source file: %s\n", filepath);
    printf("============================================================\n\n");

    /* Run Phase 1: Scanning and Phase 2: Parsing in sequence */
    ParseNode *tree = parse_file(filepath);

    /* Print the parse tree generated in Phase 2 */
    printf("=== PARSE TREE ===\n");
    print_tree(tree, 0);
    printf("\n");

    /* Check for parse errors */
    if (parse_error_count > 0)
    {
        printf("Parse complete: %d syntax error(s) found. "
               "Skipping IC generation.\n",
               parse_error_count);
        free_tree(tree);
        return 1;
    }

    printf("Parse complete: NO syntax errors found.\n\n");

    /* Phase 3: Intermediate Code Generation by calling icg_generate that walks the parse tree and prints TAC */
    icg_generate(tree);

    /* Free allocated memory and exit */
    free_tree(tree);
    return 0;
}