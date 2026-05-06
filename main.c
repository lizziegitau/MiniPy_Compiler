/* main.c  —  MiniPy Compiler  (Group 8)
 *
 *  Phase 1: Lexical Analysis  (scanner.c)
 *  Phase 2: Syntax Analysis   (parser.c)
 *  Phase 3: IC Generation     (icg.c)       ← NEW
 */

#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"
#include "parser.h"
#include "icg.h" /* Phase 3 */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source.minipy>\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];

    printf("============================================================\n");
    printf("  MiniPy Compiler  —  Group 8\n");
    printf("  Source file: %s\n", filepath);
    printf("============================================================\n\n");

    /* ── Phase 1 + 2: scan and parse ──────────────────────────────────── */
    ParseNode *tree = parse_file(filepath); /* token log printed inside  */

    /* ── Phase 2 output: parse tree ───────────────────────────────────── */
    printf("=== PARSE TREE ===\n");
    print_tree(tree, 0);
    printf("\n");

    if (parse_error_count > 0)
    {
        printf("Parse complete: %d syntax error(s) found. "
               "Skipping IC generation.\n",
               parse_error_count);
        free_tree(tree);
        return 1;
    }

    printf("Parse complete: NO syntax errors found.\n\n");

    /* ── Phase 3: intermediate code generation ─────────────────────────── */
    icg_generate(tree); /* walks the parse tree and prints TAC           */

    /* ── Cleanup ───────────────────────────────────────────────────────── */
    free_tree(tree);
    return 0;
}