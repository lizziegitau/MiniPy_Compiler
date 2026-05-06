#ifndef ICG_H
#define ICG_H

#include "parser.h"

/* ─────────────────────────────────────────────────────────────────────────────
   icg.h  —  MiniPy Intermediate Code Generator (Phase 3)
   Representation: Three-Address Code stored as quadruples (Quad).

   Each quadruple has the form:
       result = arg1  OP  arg2       (binary)
       result = OP  arg1             (unary / assign)
       GOTO label                    (unconditional jump)
       IF_FALSE result GOTO label    (conditional jump)
       LABEL L0                      (label definition)
       PRINT arg1                    (output)
   ───────────────────────────────────────────────────────────────────────────*/

#define MAX_QUADS 1024 /* maximum number of TAC instructions              */
#define MAX_OP 24      /* max length of an operator string  e.g. "IF_FALSE_GOTO" */
#define MAX_OPERAND 64 /* max length of a variable / literal / label name */

/* One quadruple ─────────────────────────────────────────────────────────── */
typedef struct
{
    char op[MAX_OP];          /* operator:  ADD, SUB, MUL, DIV, NEG,
                                            LT, GT, LEQ, GEQ, EQ, NEQ, NOT,
                                            AND, OR,
                                            ASSIGN, PRINT,
                                            LABEL, GOTO, IF_FALSE_GOTO       */
    char arg1[MAX_OPERAND];   /* left operand  (empty string if unused)      */
    char arg2[MAX_OPERAND];   /* right operand (empty string if unused)      */
    char result[MAX_OPERAND]; /* destination temp / variable / label target  */
} Quad;

/* Global quad array ─────────────────────────────────────────────────────── */
extern Quad quads[MAX_QUADS];
extern int quad_count;

/* Public API ────────────────────────────────────────────────────────────── */

/* Entry point: walk the parse tree rooted at `root` and fill quads[].      */
void icg_generate(ParseNode *root);

/* Print the full TAC listing to stdout.                                     */
void icg_print(void);

#endif /* ICG_H */