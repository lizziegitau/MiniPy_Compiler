#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"
#include "parser.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source.minipy>\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];

    printf("============================================================\n");
    printf("  MiniPy Compiler  Group 8\n");
    printf("  Source file: %s\n", filepath);
    printf("============================================================\n\n");

    // Phase 1 + 2: Scan and parse
    ParseNode *tree = parse_file(filepath); /* logs tokens internally */

    // Phase 3: Print parse tree
    printf("=== PARSE TREE ===\n");
    print_tree(tree, 0);
    printf("\n");

    if (parse_error_count == 0)
        printf("Parse complete: NO syntax errors found.\n");
    else
        printf("Parse complete: %d syntax error(s) found.\n", parse_error_count);

    // Cleanup
    free_tree(tree);
    return (parse_error_count == 0) ? 0 : 1;
}